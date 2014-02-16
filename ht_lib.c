#include "ht_p.h"
#pragma GCC diagnostic ignored "-Waddress"

/* return the hexadecimal Pth library version number */
long ht_version(void)
{
    return HT_VERSION;
}

/* implicit initialization support */
int ht_initialized = FALSE;

/* initialize the package */
int 
ht_init(void)
{
    ht_attr_t t_attr;

    /* support for implicit initialization calls
       and to prevent multiple explict initialization, too */
    if (ht_initialized)
        return ht_error(FALSE, EPERM);
    else
        ht_initialized = TRUE;

    ht_debug1("ht_init: enter");

    /* initialize the scheduler */
    if (!ht_scheduler_init()) {
        return ht_error(FALSE, EAGAIN);
    }

    /* spawn the scheduler thread */
    t_attr = ht_attr_new();
    ht_attr_set(t_attr, HT_ATTR_PRIO,         HT_PRIO_MAX);
    ht_attr_set(t_attr, HT_ATTR_NAME,         "**SCHEDULER**");
    ht_attr_set(t_attr, HT_ATTR_JOINABLE,     FALSE);
    ht_attr_set(t_attr, HT_ATTR_CANCEL_STATE, HT_CANCEL_DISABLE);
    ht_attr_set(t_attr, HT_ATTR_STACK_SIZE,   64*1024);
    ht_attr_set(t_attr, HT_ATTR_STACK_ADDR,   NULL);
    ht_sched = ht_spawn(t_attr, ht_scheduler, NULL);
    if (ht_sched == NULL) {
        ht_shield {
            ht_attr_destroy(t_attr);
            ht_scheduler_kill();
        }
        return FALSE;
    }

    /* spawn a thread for the main program */
    ht_attr_set(t_attr, HT_ATTR_PRIO,         HT_PRIO_STD);
    ht_attr_set(t_attr, HT_ATTR_NAME,         "main");
    ht_attr_set(t_attr, HT_ATTR_JOINABLE,     TRUE);
    ht_attr_set(t_attr, HT_ATTR_CANCEL_STATE, HT_CANCEL_ENABLE|HT_CANCEL_DEFERRED);
    ht_attr_set(t_attr, HT_ATTR_STACK_SIZE,   0 /* special */);
    ht_attr_set(t_attr, HT_ATTR_STACK_ADDR,   NULL);
    ht_main = ht_spawn(t_attr, (void *(*)(void *))(-1), NULL);
    if (ht_main == NULL) {
        ht_shield {
            ht_attr_destroy(t_attr);
            ht_scheduler_kill();
        }
        return FALSE;
    }
    ht_attr_destroy(t_attr);

    /*
     * The first time we've to manually switch into the scheduler to start
     * threading. Because at this time the only non-scheduler thread is the
     * "main thread" we will come back immediately. We've to also initialize
     * the ht_current variable here to allow the ht_spawn_trampoline
     * function to find the scheduler.
     */
    ht_current = ht_sched;
    ht_mctx_switch(&ht_main->mctx, &ht_sched->mctx);

    /* came back, so let's go home... */
    ht_debug1("ht_init: leave");
    return TRUE;
}

/* kill the package internals */
int 
ht_kill(void)
{
    if (!ht_initialized)
        return ht_error(FALSE, EINVAL);
    if (ht_current != ht_main)
        return ht_error(FALSE, EPERM);
    ht_debug1("ht_kill: enter");
    ht_thread_cleanup(ht_main);
    ht_scheduler_kill();
    ht_initialized = FALSE;
    ht_tcb_free(ht_sched);
    ht_tcb_free(ht_main);
    ht_debug1("ht_kill: leave");
    return TRUE;
}

/* scheduler control/query */
long 
ht_ctrl(unsigned long query, ...)
{
    long rc;
    va_list ap;

    rc = 0;
    va_start(ap, query);
    if (query & HT_CTRL_GETTHREADS) {
        if (query & HT_CTRL_GETTHREADS_NEW)
            rc += ht_pqueue_elements(&ht_NQ);
        if (query & HT_CTRL_GETTHREADS_READY)
            rc += ht_pqueue_elements(&ht_RQ);
        if (query & HT_CTRL_GETTHREADS_RUNNING)
            rc += 1; /* ht_current only */
        if (query & HT_CTRL_GETTHREADS_WAITING)
            rc += ht_pqueue_elements(&ht_WQ);
        if (query & HT_CTRL_GETTHREADS_SUSPENDED)
            rc += ht_pqueue_elements(&ht_SQ);
        if (query & HT_CTRL_GETTHREADS_DEAD)
            rc += ht_pqueue_elements(&ht_DQ);
    }
    else if (query & HT_CTRL_GETAVLOAD) {
        float *pload = va_arg(ap, float *);
        *pload = ht_loadval;
    }
    else if (query & HT_CTRL_GETPRIO) {
        ht_t t = va_arg(ap, ht_t);
        rc = t->prio;
    }
    else if (query & HT_CTRL_GETNAME) {
        ht_t t = va_arg(ap, ht_t);
        rc = (long)t->name;
    }
    else if (query & HT_CTRL_DUMPSTATE) {
        FILE *fp = va_arg(ap, FILE *);
        ht_dumpstate(fp);
    }
    else if (query & HT_CTRL_FAVOURNEW) {
        int favournew = va_arg(ap, int);
        ht_favournew = (favournew ? 1 : 0);
    }
    else
        rc = -1;
    va_end(ap);
    if (rc == -1)
        return ht_error(-1, EINVAL);
    return rc;
}

/* create a new thread of execution by spawning a cooperative thread */
static 
void 
ht_spawn_trampoline(void)
{
    void *data;

    /* just jump into the start routine */
    data = (*ht_current->start_func)(ht_current->start_arg);

    /* and do an implicit exit of the thread with the result value */
    ht_exit(data);

    /* NOTREACHED */
    abort();
}

ht_t 
ht_spawn(ht_attr_t attr, void *(*func)(void *), void *arg)
{
    ht_t t;
    unsigned int stacksize;
    void *stackaddr;
    ht_time_t ts;

    ht_debug1("ht_spawn: enter");

    /* consistency */
    if (func == NULL)
        return ht_error((ht_t)NULL, EINVAL);

    /* support the special case of main() */
    if (func == (void *(*)(void *))(-1))
        func = NULL;

    /* allocate a new thread control block */
    stacksize = (attr == HT_ATTR_DEFAULT ? 64*1024 : attr->a_stacksize);
    stackaddr = (attr == HT_ATTR_DEFAULT ? NULL    : attr->a_stackaddr);
    if ((t = ht_tcb_alloc(stacksize, stackaddr)) == NULL)
        return ht_error((ht_t)NULL, errno);

    /* configure remaining attributes */
    if (attr != HT_ATTR_DEFAULT) {
        /* overtake fields from the attribute structure */
        t->prio        = attr->a_prio;
        t->joinable    = attr->a_joinable;
        t->cancelstate = attr->a_cancelstate;
        t->dispatches  = attr->a_dispatches;
        ht_util_cpystrn(t->name, attr->a_name, HT_TCB_NAMELEN);
    }
    else if (ht_current != NULL) {
        /* overtake some fields from the parent thread */
        t->prio        = ht_current->prio;
        t->joinable    = ht_current->joinable;
        t->cancelstate = ht_current->cancelstate;
        t->dispatches  = 0;
        ht_snprintf(t->name, HT_TCB_NAMELEN, "%s.child@%d=0x%lx",
                     ht_current->name, (unsigned int)time(NULL),
                     (unsigned long)ht_current);
    }
    else {
        /* defaults */
        t->prio        = HT_PRIO_STD;
        t->joinable    = TRUE;
        t->cancelstate = HT_CANCEL_DEFAULT;
        t->dispatches  = 0;
        ht_snprintf(t->name, HT_TCB_NAMELEN,
                     "user/%x", (unsigned int)time(NULL));
    }

    /* initialize the time points and ranges */
    ht_time_set(&ts, HT_TIME_NOW);
    ht_time_set(&t->spawned, &ts);
    ht_time_set(&t->lastran, &ts);
    ht_time_set(&t->running, HT_TIME_ZERO);

    /* initialize events */
    t->events = NULL;

    /* remember the start routine and arguments for our trampoline */
    t->start_func = func;
    t->start_arg  = arg;

    /* initialize join argument */
    t->join_arg = NULL;

    /* initialize thread specific storage */
    t->data_value = NULL;
    t->data_count = 0;

    /* initialize cancellation stuff */
    t->cancelreq   = FALSE;
    t->cleanups    = NULL;

    /* initialize mutex stuff */
    ht_ring_init(&t->mutexring);

    /* initialize the machine context of this new thread */
    if (t->stacksize > 0) { /* the "main thread" (indicated by == 0) is special! */
        if (!ht_mctx_set(&t->mctx, ht_spawn_trampoline,
                          t->stack, ((char *)t->stack+t->stacksize))) {
            ht_shield { ht_tcb_free(t); }
            return ht_error((ht_t)NULL, errno);
        }
    }

    /* finally insert it into the "new queue" where
       the scheduler will pick it up for dispatching */
    if (func != ht_scheduler) {
        t->state = HT_STATE_NEW;
        ht_pqueue_insert(&ht_NQ, t->prio, t);
    }

    ht_debug1("ht_spawn: leave");

    /* the returned thread id is just the pointer
       to the thread control block... */
    return t;
}

/* returns the current thread */
ht_t 
ht_self(void)
{
    return ht_current;
}

/* check whether a thread exists */
int 
ht_thread_exists(ht_t t)
{
    if (!ht_pqueue_contains(&ht_NQ, t))
        if (!ht_pqueue_contains(&ht_RQ, t))
            if (!ht_pqueue_contains(&ht_WQ, t))
                if (!ht_pqueue_contains(&ht_SQ, t))
                    if (!ht_pqueue_contains(&ht_DQ, t))
                        return ht_error(FALSE, ESRCH); /* not found */
    return TRUE;
}

/* cleanup a particular thread */
void 
ht_thread_cleanup(ht_t thread)
{
    /* run the cleanup handlers */
    if (thread->cleanups != NULL)
        ht_cleanup_popall(thread, TRUE);

    /* run the specific data destructors */
    if (thread->data_value != NULL)
        ht_key_destroydata(thread);

    /* release still acquired mutex variables */
    ht_mutex_releaseall(thread);

    return;
}

/* terminate the current thread */
static 
int 
ht_exit_cb(void *arg)
{
    int rc;

    /* BE CAREFUL HERE: THIS FUNCTION EXECUTES
       FROM WITHIN THE _SCHEDULER_ THREAD! */

    /* calculate number of still existing threads in system. Only
       skipped queue is ht_DQ (dead queue). This queue does not
       count here, because those threads are non-detached but already
       terminated ones -- and if we are the only remaining thread (which
       also wants to terminate and not join those threads) we can signal
       us through the scheduled event (for which we are running as the
       test function inside the scheduler) that the whole process can
       terminate now. */
    rc = 0;
    rc += ht_pqueue_elements(&ht_NQ);
    rc += ht_pqueue_elements(&ht_RQ);
    rc += ht_pqueue_elements(&ht_WQ);
    rc += ht_pqueue_elements(&ht_SQ);

    if (rc == 1 /* just our main thread */)
        return TRUE;
    else
        return FALSE;
}

void 
ht_exit(void *value)
{
    ht_event_t ev;

    ht_debug2("ht_exit: marking thread \"%s\" as dead", ht_current->name);

    /* the main thread is special, because its termination
       would terminate the whole process, so we have to delay 
       its termination until it is really the last thread */
    if (ht_current == ht_main) {
        if (!ht_exit_cb(NULL)) {
            ev = ht_event(HT_EVENT_FUNC, ht_exit_cb);
            ht_wait(ev);
            ht_event_free(ev, HT_FREE_THIS);
        }
    }

    /* execute cleanups */
    ht_thread_cleanup(ht_current);

    if (ht_current != ht_main) {
        /*
         * Now mark the current thread as dead, explicitly switch into the
         * scheduler and let it reap the current thread structure; we can't
         * free it here, or we'd be running on a stack which malloc() regards
         * as free memory, which would be a somewhat perilous situation.
         */
        ht_current->join_arg = value;
        ht_current->state = HT_STATE_DEAD;
        ht_debug2("ht_exit: switching from thread \"%s\" to scheduler", ht_current->name);
        ht_mctx_switch(&ht_current->mctx, &ht_sched->mctx);
    }
    else {
        /*
         * main thread is special: exit the _process_
         * [double-casted to avoid warnings because of size]
         */
        ht_kill();
        exit((int)((long)value));
    }

    /* NOTREACHED */
    abort();
}

/* waits for the termination of the specified thread */
int 
ht_join(ht_t tid, void **value)
{
    ht_event_t ev;
    static ht_key_t ev_key = HT_KEY_INIT;

    ht_debug2("ht_join: joining thread \"%s\"", tid == NULL ? "-ANY-" : tid->name);
    if (tid == ht_current)
        return ht_error(FALSE, EDEADLK);
    if (tid != NULL && !tid->joinable)
        return ht_error(FALSE, EINVAL);
    if (ht_ctrl(HT_CTRL_GETTHREADS) == 1)
        return ht_error(FALSE, EDEADLK);
    if (tid == NULL)
        tid = ht_pqueue_head(&ht_DQ);
    if (tid == NULL || (tid != NULL && tid->state != HT_STATE_DEAD)) {
        ev = ht_event(HT_EVENT_TID|HT_UNTIL_TID_DEAD|HT_MODE_STATIC, &ev_key, tid);
        ht_wait(ev);
    }
    if (tid == NULL)
        tid = ht_pqueue_head(&ht_DQ);
    if (tid == NULL || (tid != NULL && tid->state != HT_STATE_DEAD))
        return ht_error(FALSE, EIO);
    if (value != NULL)
        *value = tid->join_arg;
    ht_pqueue_delete(&ht_DQ, tid);
    ht_tcb_free(tid);
    return TRUE;
}

/* delegates control back to scheduler for context switches */
int 
ht_yield(ht_t to)
{
    ht_pqueue_t *q = NULL;

    ht_debug2("ht_yield: enter from thread \"%s\"", ht_current->name);

    /* a given thread has to be new or ready or we ignore the request */
    if (to != NULL) {
        switch (to->state) {
            case HT_STATE_NEW:    q = &ht_NQ; break;
            case HT_STATE_READY:  q = &ht_RQ; break;
            default:               q = NULL;
        }
        if (q == NULL || !ht_pqueue_contains(q, to))
            return ht_error(FALSE, EINVAL);
    }

    /* give a favored thread maximum priority in his queue */
    if (to != NULL && q != NULL)
        ht_pqueue_favorite(q, to);

    /* switch to scheduler */
    if (to != NULL)
        ht_debug2("ht_yield: give up control to scheduler "
                   "in favour of thread \"%s\"", to->name);
    else
        ht_debug1("ht_yield: give up control to scheduler");
    ht_mctx_switch(&ht_current->mctx, &ht_sched->mctx);
    ht_debug1("ht_yield: got back control from scheduler");

    ht_debug2("ht_yield: leave to thread \"%s\"", ht_current->name);
    return TRUE;
}

/* suspend a thread until its again manually resumed */
int 
ht_suspend(ht_t t)
{
    ht_pqueue_t *q;

    if (t == NULL)
        return ht_error(FALSE, EINVAL);
    if (t == ht_sched || t == ht_current)
        return ht_error(FALSE, EPERM);
    switch (t->state) {
        case HT_STATE_NEW:     q = &ht_NQ; break;
        case HT_STATE_READY:   q = &ht_RQ; break;
        case HT_STATE_WAITING: q = &ht_WQ; break;
        default:                q = NULL;
    }
    if (q == NULL)
        return ht_error(FALSE, EPERM);
    if (!ht_pqueue_contains(q, t))
        return ht_error(FALSE, ESRCH);
    ht_pqueue_delete(q, t);
    ht_pqueue_insert(&ht_SQ, HT_PRIO_STD, t);
    ht_debug2("ht_suspend: suspend thread \"%s\"\n", t->name);
    return TRUE;
}

/* resume a previously suspended thread */
int 
ht_resume(ht_t t)
{
    ht_pqueue_t *q;

    if (t == NULL)
        return ht_error(FALSE, EINVAL);
    if (t == ht_sched || t == ht_current)
        return ht_error(FALSE, EPERM);
    if (!ht_pqueue_contains(&ht_SQ, t))
        return ht_error(FALSE, EPERM);
    ht_pqueue_delete(&ht_SQ, t);
    switch (t->state) {
        case HT_STATE_NEW:     q = &ht_NQ; break;
        case HT_STATE_READY:   q = &ht_RQ; break;
        case HT_STATE_WAITING: q = &ht_WQ; break;
        default:                q = NULL;
    }
    ht_pqueue_insert(q, HT_PRIO_STD, t);
    ht_debug2("ht_resume: resume thread \"%s\"\n", t->name);
    return TRUE;
}

/* switch a filedescriptor's I/O mode */
int 
ht_fdmode(int fd, int newmode)
{
    int fdmode;
    int oldmode;

    /* retrieve old mode (usually a very cheap operation) */
    if ((fdmode = fcntl(fd, F_GETFL, NULL)) == -1)
        oldmode = HT_FDMODE_ERROR;
    else if (fdmode & O_NONBLOCKING)
        oldmode = HT_FDMODE_NONBLOCK;
    else
        oldmode = HT_FDMODE_BLOCK;

    /* set new mode (usually a more expensive operation) */
    if (oldmode == HT_FDMODE_BLOCK && newmode == HT_FDMODE_NONBLOCK)
        fcntl(fd, F_SETFL, (fdmode | O_NONBLOCKING));
    if (oldmode == HT_FDMODE_NONBLOCK && newmode == HT_FDMODE_BLOCK)
        fcntl(fd, F_SETFL, (fdmode & ~(O_NONBLOCKING)));

    /* return old mode */
    return oldmode;
}

/* wait for specific amount of time */
int 
ht_nap(ht_time_t naptime)
{
    ht_time_t until;
    ht_event_t ev;
    static ht_key_t ev_key = HT_KEY_INIT;

    if (ht_time_cmp(&naptime, HT_TIME_ZERO) == 0)
        return ht_error(FALSE, EINVAL);
    ht_time_set(&until, HT_TIME_NOW);
    ht_time_add(&until, &naptime);
    ev = ht_event(HT_EVENT_TIME|HT_MODE_STATIC, &ev_key, until);
    ht_wait(ev);
    return TRUE;
}

/* runs a constructor once */
int 
ht_once(ht_once_t *oncectrl, void (*constructor)(void *), void *arg)
{
    if (oncectrl == NULL || constructor == NULL)
        return ht_error(FALSE, EINVAL);
    if (*oncectrl != TRUE)
        constructor(arg);
    *oncectrl = TRUE;
    return TRUE;
}


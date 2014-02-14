#include "ht_p.h"

/* event structure destructor */
static void ht_event_destructor(void *vp)
{
    /* free this single(!) event. That it is just a single event is a
       requirement for ht_event(HT_MODE_STATIC, ...), or else we would
       get into horrible trouble on asychronous cleanups */
    ht_event_free((ht_event_t)vp, HT_FREE_THIS);
    return;
}

/* event structure constructor */
ht_event_t ht_event(unsigned long spec, ...)
{
    ht_event_t ev;
    ht_key_t *ev_key;
    va_list ap;

    va_start(ap, spec);

    /* allocate new or reuse static or supplied event structure */
    if (spec & HT_MODE_REUSE) {
        /* reuse supplied event structure */
        ev = va_arg(ap, ht_event_t);
    }
    else if (spec & HT_MODE_STATIC) {
        /* reuse static event structure */
        ev_key = va_arg(ap, ht_key_t *);
        if (*ev_key == HT_KEY_INIT)
            ht_key_create(ev_key, ht_event_destructor);
        ev = (ht_event_t)ht_key_getdata(*ev_key);
        if (ev == NULL) {
            ev = (ht_event_t)malloc(sizeof(struct ht_event_st));
            ht_key_setdata(*ev_key, ev);
        }
    }
    else {
        /* allocate new dynamic event structure */
        ev = (ht_event_t)malloc(sizeof(struct ht_event_st));
    }
    if (ev == NULL)
        return ht_error((ht_event_t)NULL, errno);

    /* create new event ring out of event or insert into existing ring */
    if (spec & HT_MODE_CHAIN) {
        ht_event_t ch = va_arg(ap, ht_event_t);
        ev->ev_prev = ch->ev_prev;
        ev->ev_next = ch;
        ev->ev_prev->ev_next = ev;
        ev->ev_next->ev_prev = ev;
    }
    else {
        ev->ev_prev = ev;
        ev->ev_next = ev;
    }

    /* initialize common ingredients */
    ev->ev_status = HT_STATUS_PENDING;

    /* initialize event specific ingredients */
    if (spec & HT_EVENT_FD) {
        /* filedescriptor event */
        int fd = va_arg(ap, int);
        if (!ht_util_fd_valid(fd))
            return ht_error((ht_event_t)NULL, EBADF);
        ev->ev_type = HT_EVENT_FD;
        ev->ev_goal = (int)(spec & (HT_UNTIL_FD_READABLE|\
                                    HT_UNTIL_FD_WRITEABLE|\
                                    HT_UNTIL_FD_EXCEPTION));
        ev->ev_args.FD.fd = fd;
    }
    else if (spec & HT_EVENT_SELECT) {
        /* filedescriptor set select event */
        int *n = va_arg(ap, int *);
        int nfd = va_arg(ap, int);
        fd_set *rfds = va_arg(ap, fd_set *);
        fd_set *wfds = va_arg(ap, fd_set *);
        fd_set *efds = va_arg(ap, fd_set *);
        ev->ev_type = HT_EVENT_SELECT;
        ev->ev_goal = (int)(spec & (HT_UNTIL_OCCURRED));
        ev->ev_args.SELECT.n    = n;
        ev->ev_args.SELECT.nfd  = nfd;
        ev->ev_args.SELECT.rfds = rfds;
        ev->ev_args.SELECT.wfds = wfds;
        ev->ev_args.SELECT.efds = efds;
    }
    else if (spec & HT_EVENT_TIME) {
        /* interrupt request event */
        ht_time_t tv = va_arg(ap, ht_time_t);
        ev->ev_type = HT_EVENT_TIME;
        ev->ev_goal = (int)(spec & (HT_UNTIL_OCCURRED));
        ev->ev_args.TIME.tv = tv;
    }
    else if (spec & HT_EVENT_MSG) {
        /* message port event */
        ht_msgport_t mp = va_arg(ap, ht_msgport_t);
        ev->ev_type = HT_EVENT_MSG;
        ev->ev_goal = (int)(spec & (HT_UNTIL_OCCURRED));
        ev->ev_args.MSG.mp = mp;
    }
    else if (spec & HT_EVENT_MUTEX) {
        /* mutual exclusion lock */
        ht_mutex_t *mutex = va_arg(ap, ht_mutex_t *);
        ev->ev_type = HT_EVENT_MUTEX;
        ev->ev_goal = (int)(spec & (HT_UNTIL_OCCURRED));
        ev->ev_args.MUTEX.mutex = mutex;
    }
    else if (spec & HT_EVENT_COND) {
        /* condition variable */
        ht_cond_t *cond = va_arg(ap, ht_cond_t *);
        ev->ev_type = HT_EVENT_COND;
        ev->ev_goal = (int)(spec & (HT_UNTIL_OCCURRED));
        ev->ev_args.COND.cond = cond;
    }
    else if (spec & HT_EVENT_TID) {
        /* thread id event */
        ht_t tid = va_arg(ap, ht_t);
        int goal;
        ev->ev_type = HT_EVENT_TID;
        if (spec & HT_UNTIL_TID_NEW)
            goal = HT_STATE_NEW;
        else if (spec & HT_UNTIL_TID_READY)
            goal = HT_STATE_READY;
        else if (spec & HT_UNTIL_TID_WAITING)
            goal = HT_STATE_WAITING;
        else if (spec & HT_UNTIL_TID_DEAD)
            goal = HT_STATE_DEAD;
        else
            goal = HT_STATE_READY;
        ev->ev_goal = goal;
        ev->ev_args.TID.tid = tid;
    }
    else if (spec & HT_EVENT_FUNC) {
        /* custom function event */
        ev->ev_type = HT_EVENT_FUNC;
        ev->ev_goal = (int)(spec & (HT_UNTIL_OCCURRED));
        ev->ev_args.FUNC.func  = va_arg(ap, ht_event_func_t);
        ev->ev_args.FUNC.arg   = va_arg(ap, void *);
        ev->ev_args.FUNC.tv    = va_arg(ap, ht_time_t);
    }
    else
        return ht_error((ht_event_t)NULL, EINVAL);

    va_end(ap);

    /* return event */
    return ev;
}

/* determine type of event */
unsigned long ht_event_typeof(ht_event_t ev)
{
    if (ev == NULL)
        return ht_error(0, EINVAL);
    return (ev->ev_type | ev->ev_goal);
}

/* event extractor */
int ht_event_extract(ht_event_t ev, ...)
{
    va_list ap;

    if (ev == NULL)
        return ht_error(FALSE, EINVAL);
    va_start(ap, ev);

    /* extract event specific ingredients */
    if (ev->ev_type & HT_EVENT_FD) {
        /* filedescriptor event */
        int *fd = va_arg(ap, int *);
        *fd = ev->ev_args.FD.fd;
    }
    else if (ev->ev_type & HT_EVENT_TIME) {
        /* interrupt request event */
        ht_time_t *tv = va_arg(ap, ht_time_t *);
        *tv = ev->ev_args.TIME.tv;
    }
    else if (ev->ev_type & HT_EVENT_MSG) {
        /* message port event */
        ht_msgport_t *mp = va_arg(ap, ht_msgport_t *);
        *mp = ev->ev_args.MSG.mp;
    }
    else if (ev->ev_type & HT_EVENT_MUTEX) {
        /* mutual exclusion lock */
        ht_mutex_t **mutex = va_arg(ap, ht_mutex_t **);
        *mutex = ev->ev_args.MUTEX.mutex;
    }
    else if (ev->ev_type & HT_EVENT_COND) {
        /* condition variable */
        ht_cond_t **cond = va_arg(ap, ht_cond_t **);
        *cond = ev->ev_args.COND.cond;
    }
    else if (ev->ev_type & HT_EVENT_TID) {
        /* thread id event */
        ht_t *tid = va_arg(ap, ht_t *);
        *tid = ev->ev_args.TID.tid;
    }
    else if (ev->ev_type & HT_EVENT_FUNC) {
        /* custom function event */
        ht_event_func_t *func = va_arg(ap, ht_event_func_t *);
        void **arg             = va_arg(ap, void **);
        ht_time_t *tv         = va_arg(ap, ht_time_t *);
        *func = ev->ev_args.FUNC.func;
        *arg  = ev->ev_args.FUNC.arg;
        *tv   = ev->ev_args.FUNC.tv;
    }
    else
        return ht_error(FALSE, EINVAL);
    va_end(ap);
    return TRUE;
}

/* concatenate one or more events or event rings */
ht_event_t ht_event_concat(ht_event_t evf, ...)
{
    ht_event_t evc; /* current event */
    ht_event_t evn; /* next event */
    ht_event_t evl; /* last event */
    ht_event_t evt; /* temporary event */
    va_list ap;

    if (evf == NULL)
        return ht_error((ht_event_t)NULL, EINVAL);

    /* open ring */
    va_start(ap, evf);
    evc = evf;
    evl = evc->ev_next;

    /* attach additional rings */
    while ((evn = va_arg(ap, ht_event_t)) != NULL) {
        evc->ev_next = evn;
        evt = evn->ev_prev;
        evn->ev_prev = evc;
        evc = evt;
    }

    /* close ring */
    evc->ev_next = evl;
    evl->ev_prev = evc;
    va_end(ap);

    return evf;
}

/* isolate one event from a possible appended event ring */
ht_event_t ht_event_isolate(ht_event_t ev)
{
    ht_event_t ring;

    if (ev == NULL)
        return ht_error((ht_event_t)NULL, EINVAL);
    ring = NULL;
    if (!(ev->ev_next == ev && ev->ev_prev == ev)) {
        ring = ev->ev_next;
        ev->ev_prev->ev_next = ev->ev_next;
        ev->ev_next->ev_prev = ev->ev_prev;
        ev->ev_prev = ev;
        ev->ev_next = ev;
    }
    return ring;
}

/* determine status of the event */
ht_status_t ht_event_status(ht_event_t ev)
{
    if (ev == NULL)
        return ht_error(FALSE, EINVAL);
    return ev->ev_status;
}

/* walk to next or previous event in an event ring */
ht_event_t ht_event_walk(ht_event_t ev, unsigned int direction)
{
    if (ev == NULL)
        return ht_error((ht_event_t)NULL, EINVAL);
    do {
        if (direction & HT_WALK_NEXT)
            ev = ev->ev_next;
        else if (direction & HT_WALK_PREV)
            ev = ev->ev_prev;
        else
            return ht_error((ht_event_t)NULL, EINVAL);
    } while ((direction & HT_UNTIL_OCCURRED) && (ev->ev_status != HT_STATUS_OCCURRED));
    return ev;
}

/* deallocate an event structure */
int ht_event_free(ht_event_t ev, int mode)
{
    ht_event_t evc;
    ht_event_t evn;

    if (ev == NULL)
        return ht_error(FALSE, EINVAL);
    if (mode == HT_FREE_THIS) {
        ev->ev_prev->ev_next = ev->ev_next;
        ev->ev_next->ev_prev = ev->ev_prev;
        free(ev);
    }
    else if (mode == HT_FREE_ALL) {
        evc = ev;
        do {
            evn = evc->ev_next;
            free(evc);
            evc = evn;
        } while (evc != ev);
    }
    return TRUE;
}

/* wait for one or more events */
int ht_wait(ht_event_t ev_ring)
{
    int nonpending;
    ht_event_t ev;

    /* at least a waiting ring is required */
    if (ev_ring == NULL)
        return ht_error(-1, EINVAL);
    ht_debug2("ht_wait: enter from thread \"%s\"", ht_current->name);

    /* mark all events in waiting ring as still pending */
    ev = ev_ring;
    do {
        ev->ev_status = HT_STATUS_PENDING;
        ht_debug2("ht_wait: waiting on event 0x%lx", (unsigned long)ev);
        ev = ev->ev_next;
    } while (ev != ev_ring);

    /* link event ring to current thread */
    ht_current->events = ev_ring;

    /* move thread into waiting state
       and transfer control to scheduler */
    ht_current->state = HT_STATE_WAITING;
    ht_yield(NULL);

    /* check for cancellation */
    ht_cancel_point();

    /* unlink event ring from current thread */
    ht_current->events = NULL;

    /* count number of actually occurred (or failed) events */
    ev = ev_ring;
    nonpending = 0;
    do {
        if (ev->ev_status != HT_STATUS_PENDING) {
            ht_debug2("ht_wait: non-pending event 0x%lx", (unsigned long)ev);
            nonpending++;
        }
        ev = ev->ev_next;
    } while (ev != ev_ring);

    /* leave to current thread with number of occurred events */
    ht_debug2("ht_wait: leave to thread \"%s\"", ht_current->name);
    return nonpending;
}


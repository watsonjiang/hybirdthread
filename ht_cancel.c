#include "ht_p.h"

/* set cancellation state */
void ht_cancel_state(int newstate, int *oldstate)
{
    if (oldstate != NULL)
        *oldstate = ht_current->cancelstate;
    if (newstate != 0)
        ht_current->cancelstate = newstate;
    return;
}

/* enter a cancellation point */
void ht_cancel_point(void)
{
    if (   ht_current->cancelreq == TRUE
        && ht_current->cancelstate & HT_CANCEL_ENABLE) {
        /* avoid looping if cleanup handlers contain cancellation points */
        ht_current->cancelreq = FALSE;
        ht_debug2("ht_cancel_point: terminating cancelled thread \"%s\"", ht_current->name);
        ht_exit(HT_CANCELED);
    }
    return;
}

/* cancel a thread (the friendly way) */
int ht_cancel(ht_t thread)
{
    ht_pqueue_t *q;

    if (thread == NULL)
        return ht_error(FALSE, EINVAL);

    /* the current thread cannot be cancelled */
    if (thread == ht_current)
        return ht_error(FALSE, EINVAL);

    /* the thread has to be at least still alive */
    if (thread->state == HT_STATE_DEAD)
        return ht_error(FALSE, EPERM);

    /* now mark the thread as cancelled */
    thread->cancelreq = TRUE;

    /* when cancellation is enabled in async mode we cancel the thread immediately */
    if (   thread->cancelstate & HT_CANCEL_ENABLE
        && thread->cancelstate & HT_CANCEL_ASYNCHRONOUS) {

        /* remove thread from its queue */
        switch (thread->state) {
            case HT_STATE_NEW:     q = &ht_NQ; break;
            case HT_STATE_READY:   q = &ht_RQ; break;
            case HT_STATE_WAITING: q = &ht_WQ; break;
            default:                q = NULL;
        }
        if (q == NULL)
            return ht_error(FALSE, ESRCH);
        if (!ht_pqueue_contains(q, thread))
            return ht_error(FALSE, ESRCH);
        ht_pqueue_delete(q, thread);

        /* execute cleanups */
        ht_thread_cleanup(thread);

        /* and now either kick it out or move it to dead queue */
        if (!thread->joinable) {
            ht_debug2("ht_cancel: kicking out cancelled thread \"%s\" immediately", thread->name);
            ht_tcb_free(thread);
        }
        else {
            ht_debug2("ht_cancel: moving cancelled thread \"%s\" to dead queue", thread->name);
            thread->join_arg = HT_CANCELED;
            thread->state = HT_STATE_DEAD;
            ht_pqueue_insert(&ht_DQ, HT_PRIO_STD, thread);
        }
    }
    return TRUE;
}

/* abort a thread (the cruel way) */
int ht_abort(ht_t thread)
{
    if (thread == NULL)
        return ht_error(FALSE, EINVAL);

    /* the current thread cannot be aborted */
    if (thread == ht_current)
        return ht_error(FALSE, EINVAL);

    if (thread->state == HT_STATE_DEAD && thread->joinable) {
        /* if thread is already terminated, just join it */
        if (!ht_join(thread, NULL))
            return FALSE;
    }
    else {
        /* else force it to be detached and cancel it asynchronously */
        thread->joinable = FALSE;
        thread->cancelstate = (HT_CANCEL_ENABLE|HT_CANCEL_ASYNCHRONOUS);
        if (!ht_cancel(thread))
            return FALSE;
    }
    return TRUE;
}


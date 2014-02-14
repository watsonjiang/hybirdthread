#include "ht_p.h"

/*
**  Mutual Exclusion Locks
*/

int ht_mutex_init(ht_mutex_t *mutex)
{
    if (mutex == NULL)
        return ht_error(FALSE, EINVAL);
    mutex->mx_state = HT_MUTEX_INITIALIZED;
    mutex->mx_owner = NULL;
    mutex->mx_count = 0;
    return TRUE;
}

int ht_mutex_acquire(ht_mutex_t *mutex, int tryonly, ht_event_t ev_extra)
{
    static ht_key_t ev_key = HT_KEY_INIT;
    ht_event_t ev;

    ht_debug2("ht_mutex_acquire: called from thread \"%s\"", ht_current->name);

    /* consistency checks */
    if (mutex == NULL)
        return ht_error(FALSE, EINVAL);
    if (!(mutex->mx_state & HT_MUTEX_INITIALIZED))
        return ht_error(FALSE, EDEADLK);

    /* still not locked, so simply acquire mutex? */
    if (!(mutex->mx_state & HT_MUTEX_LOCKED)) {
        mutex->mx_state |= HT_MUTEX_LOCKED;
        mutex->mx_owner = ht_current;
        mutex->mx_count = 1;
        ht_ring_append(&(ht_current->mutexring), &(mutex->mx_node));
        ht_debug1("ht_mutex_acquire: immediately locking mutex");
        return TRUE;
    }

    /* already locked by caller? */
    if (mutex->mx_count >= 1 && mutex->mx_owner == ht_current) {
        /* recursive lock */
        mutex->mx_count++;
        ht_debug1("ht_mutex_acquire: recursive locking");
        return TRUE;
    }

    /* should we just tryonly? */
    if (tryonly)
        return ht_error(FALSE, EBUSY);

    /* else wait for mutex to become unlocked.. */
    ht_debug1("ht_mutex_acquire: wait until mutex is unlocked");
    for (;;) {
        ev = ht_event(HT_EVENT_MUTEX|HT_MODE_STATIC, &ev_key, mutex);
        if (ev_extra != NULL)
            ht_event_concat(ev, ev_extra, NULL);
        ht_wait(ev);
        if (ev_extra != NULL) {
            ht_event_isolate(ev);
            if (ht_event_status(ev) == HT_STATUS_PENDING)
                return ht_error(FALSE, EINTR);
        }
        if (!(mutex->mx_state & HT_MUTEX_LOCKED))
            break;
    }

    /* now it's again unlocked, so acquire mutex */
    ht_debug1("ht_mutex_acquire: locking mutex");
    mutex->mx_state |= HT_MUTEX_LOCKED;
    mutex->mx_owner = ht_current;
    mutex->mx_count = 1;
    ht_ring_append(&(ht_current->mutexring), &(mutex->mx_node));
    return TRUE;
}

int ht_mutex_release(ht_mutex_t *mutex)
{
    /* consistency checks */
    if (mutex == NULL)
        return ht_error(FALSE, EINVAL);
    if (!(mutex->mx_state & HT_MUTEX_INITIALIZED))
        return ht_error(FALSE, EDEADLK);
    if (!(mutex->mx_state & HT_MUTEX_LOCKED))
        return ht_error(FALSE, EDEADLK);
    if (mutex->mx_owner != ht_current)
        return ht_error(FALSE, EACCES);

    /* decrement recursion counter and release mutex */
    mutex->mx_count--;
    if (mutex->mx_count <= 0) {
        mutex->mx_state &= ~(HT_MUTEX_LOCKED);
        mutex->mx_owner = NULL;
        mutex->mx_count = 0;
        ht_ring_delete(&(ht_current->mutexring), &(mutex->mx_node));
    }
    return TRUE;
}

void ht_mutex_releaseall(ht_t thread)
{
    ht_ringnode_t *rn, *rnf;

    if (thread == NULL)
        return;
    /* iterate over all mutexes of thread */
    rn = rnf = ht_ring_first(&(thread->mutexring));
    while (rn != NULL) {
        ht_mutex_release((ht_mutex_t *)rn);
        rn = ht_ring_next(&(thread->mutexring), rn);
        if (rn == rnf)
            break;
    }
    return;
}

/*
**  Read-Write Locks
*/

int ht_rwlock_init(ht_rwlock_t *rwlock)
{
    if (rwlock == NULL)
        return ht_error(FALSE, EINVAL);
    rwlock->rw_state = HT_RWLOCK_INITIALIZED;
    rwlock->rw_readers = 0;
    ht_mutex_init(&(rwlock->rw_mutex_rd));
    ht_mutex_init(&(rwlock->rw_mutex_rw));
    return TRUE;
}

int ht_rwlock_acquire(ht_rwlock_t *rwlock, int op, int tryonly, ht_event_t ev_extra)
{
    /* consistency checks */
    if (rwlock == NULL)
        return ht_error(FALSE, EINVAL);
    if (!(rwlock->rw_state & HT_RWLOCK_INITIALIZED))
        return ht_error(FALSE, EDEADLK);

    /* acquire lock */
    if (op == HT_RWLOCK_RW) {
        /* read-write lock is simple */
        if (!ht_mutex_acquire(&(rwlock->rw_mutex_rw), tryonly, ev_extra))
            return FALSE;
        rwlock->rw_mode = HT_RWLOCK_RW;
    }
    else {
        /* read-only lock is more complicated to get right */
        if (!ht_mutex_acquire(&(rwlock->rw_mutex_rd), tryonly, ev_extra))
            return FALSE;
        rwlock->rw_readers++;
        if (rwlock->rw_readers == 1) {
            if (!ht_mutex_acquire(&(rwlock->rw_mutex_rw), tryonly, ev_extra)) {
                rwlock->rw_readers--;
                ht_shield { ht_mutex_release(&(rwlock->rw_mutex_rd)); }
                return FALSE;
            }
        }
        rwlock->rw_mode = HT_RWLOCK_RD;
        ht_mutex_release(&(rwlock->rw_mutex_rd));
    }
    return TRUE;
}

int ht_rwlock_release(ht_rwlock_t *rwlock)
{
    /* consistency checks */
    if (rwlock == NULL)
        return ht_error(FALSE, EINVAL);
    if (!(rwlock->rw_state & HT_RWLOCK_INITIALIZED))
        return ht_error(FALSE, EDEADLK);

    /* release lock */
    if (rwlock->rw_mode == HT_RWLOCK_RW) {
        /* read-write unlock is simple */
        if (!ht_mutex_release(&(rwlock->rw_mutex_rw)))
            return FALSE;
    }
    else {
        /* read-only unlock is more complicated to get right */
        if (!ht_mutex_acquire(&(rwlock->rw_mutex_rd), FALSE, NULL))
            return FALSE;
        rwlock->rw_readers--;
        if (rwlock->rw_readers == 0) {
            if (!ht_mutex_release(&(rwlock->rw_mutex_rw))) {
                rwlock->rw_readers++;
                ht_shield { ht_mutex_release(&(rwlock->rw_mutex_rd)); }
                return FALSE;
            }
        }
        rwlock->rw_mode = HT_RWLOCK_RD;
        ht_mutex_release(&(rwlock->rw_mutex_rd));
    }
    return TRUE;
}

/*
**  Condition Variables
*/

int ht_cond_init(ht_cond_t *cond)
{
    if (cond == NULL)
        return ht_error(FALSE, EINVAL);
    cond->cn_state   = HT_COND_INITIALIZED;
    cond->cn_waiters = 0;
    return TRUE;
}

static void ht_cond_cleanup_handler(void *_cleanvec)
{
    ht_mutex_t *mutex = (ht_mutex_t *)(((void **)_cleanvec)[0]);
    ht_cond_t  *cond  = (ht_cond_t  *)(((void **)_cleanvec)[1]);

    /* re-acquire mutex when ht_cond_await() is cancelled
       in order to restore the condition variable semantics */
    ht_mutex_acquire(mutex, FALSE, NULL);

    /* fix number of waiters */
    cond->cn_waiters--;
    return;
}

int ht_cond_await(ht_cond_t *cond, ht_mutex_t *mutex, ht_event_t ev_extra)
{
    static ht_key_t ev_key = HT_KEY_INIT;
    void *cleanvec[2];
    ht_event_t ev;

    /* consistency checks */
    if (cond == NULL || mutex == NULL)
        return ht_error(FALSE, EINVAL);
    if (!(cond->cn_state & HT_COND_INITIALIZED))
        return ht_error(FALSE, EDEADLK);

    /* check whether we can do a short-circuit wait */
    if (    (cond->cn_state & HT_COND_SIGNALED)
        && !(cond->cn_state & HT_COND_BROADCAST)) {
        cond->cn_state &= ~(HT_COND_SIGNALED);
        cond->cn_state &= ~(HT_COND_BROADCAST);
        cond->cn_state &= ~(HT_COND_HANDLED);
        return TRUE;
    }

    /* add us to the number of waiters */
    cond->cn_waiters++;

    /* release mutex (caller had to acquire it first) */
    ht_mutex_release(mutex);

    /* wait until the condition is signaled */
    ev = ht_event(HT_EVENT_COND|HT_MODE_STATIC, &ev_key, cond);
    if (ev_extra != NULL)
        ht_event_concat(ev, ev_extra, NULL);
    cleanvec[0] = mutex;
    cleanvec[1] = cond;
    ht_cleanup_push(ht_cond_cleanup_handler, cleanvec);
    ht_wait(ev);
    ht_cleanup_pop(FALSE);
    if (ev_extra != NULL)
        ht_event_isolate(ev);

    /* reacquire mutex */
    ht_mutex_acquire(mutex, FALSE, NULL);

    /* remove us from the number of waiters */
    cond->cn_waiters--;

    /* release mutex (caller had to acquire it first) */
    return TRUE;
}

int ht_cond_notify(ht_cond_t *cond, int broadcast)
{
    /* consistency checks */
    if (cond == NULL)
        return ht_error(FALSE, EINVAL);
    if (!(cond->cn_state & HT_COND_INITIALIZED))
        return ht_error(FALSE, EDEADLK);

    /* do something only if there is at least one waiters (POSIX semantics) */
    if (cond->cn_waiters > 0) {
        /* signal the condition */
        cond->cn_state |= HT_COND_SIGNALED;
        if (broadcast)
            cond->cn_state |= HT_COND_BROADCAST;
        else
            cond->cn_state &= ~(HT_COND_BROADCAST);
        cond->cn_state &= ~(HT_COND_HANDLED);

        /* and give other threads a chance to awake */
        ht_yield(NULL);
    }

    /* return to caller */
    return TRUE;
}

/*
**  Barriers
*/

int ht_barrier_init(ht_barrier_t *barrier, int threshold)
{
    if (barrier == NULL || threshold <= 0)
        return ht_error(FALSE, EINVAL);
    if (!ht_mutex_init(&(barrier->br_mutex)))
        return FALSE;
    if (!ht_cond_init(&(barrier->br_cond)))
        return FALSE;
    barrier->br_state     = HT_BARRIER_INITIALIZED;
    barrier->br_threshold = threshold;
    barrier->br_count     = threshold;
    barrier->br_cycle     = FALSE;
    return TRUE;
}

int ht_barrier_reach(ht_barrier_t *barrier)
{
    int cancel, cycle;
    int rv;

    if (barrier == NULL)
        return ht_error(FALSE, EINVAL);
    if (!(barrier->br_state & HT_BARRIER_INITIALIZED))
        return ht_error(FALSE, EINVAL);

    if (!ht_mutex_acquire(&(barrier->br_mutex), FALSE, NULL))
        return FALSE;
    cycle = barrier->br_cycle;
    if (--(barrier->br_count) == 0) {
        /* last thread reached the barrier */
        barrier->br_cycle   = !(barrier->br_cycle);
        barrier->br_count   = barrier->br_threshold;
        if ((rv = ht_cond_notify(&(barrier->br_cond), TRUE)))
            rv = HT_BARRIER_TAILLIGHT;
    }
    else {
        /* wait until remaining threads have reached the barrier, too */
        ht_cancel_state(HT_CANCEL_DISABLE, &cancel);
        if (barrier->br_threshold == barrier->br_count)
            rv = HT_BARRIER_HEADLIGHT;
        else
            rv = TRUE;
        while (cycle == barrier->br_cycle) {
            if (!(rv = ht_cond_await(&(barrier->br_cond), &(barrier->br_mutex), NULL)))
                break;
        }
        ht_cancel_state(cancel, NULL);
    }
    ht_mutex_release(&(barrier->br_mutex));
    return rv;
}


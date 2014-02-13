#include "ht_p.h"
#include <signal.h>
// to avoid warning.
#pragma GCC diagnostic ignored "-Waddress"

ht_t        ht_main;       /* the main thread                       */
ht_t        ht_sched;      /* the permanent scheduler thread        */
ht_t        ht_current;    /* the currently running thread          */
ht_pqueue_t ht_NQ;         /* queue of new threads                  */
ht_pqueue_t ht_RQ;         /* queue of threads ready to run         */
ht_pqueue_t ht_WQ;         /* queue of threads waiting for an event */
ht_pqueue_t ht_SQ;         /* queue of suspended threads            */
ht_pqueue_t ht_DQ;         /* queue of terminated threads           */
int          ht_favournew;  /* favour new threads on startup         */
float        ht_loadval;    /* average scheduler load value          */

static ht_time_t   ht_loadticknext;
static ht_time_t   ht_loadtickgap = HT_TIME(1,0);

/* initialize the scheduler ingredients */
int ht_scheduler_init(void)
{
    /* initialize the essential threads */
    ht_sched   = NULL;
    ht_current = NULL;

    /* initalize the thread queues */
    ht_pqueue_init(&ht_NQ);
    ht_pqueue_init(&ht_RQ);
    ht_pqueue_init(&ht_WQ);
    ht_pqueue_init(&ht_SQ);
    ht_pqueue_init(&ht_DQ);

    /* initialize scheduling hints */
    ht_favournew = 1; /* the default is the original behaviour */

    /* initialize load support */
    ht_loadval = 1.0;
    ht_time_set(&ht_loadticknext, HT_TIME_NOW);

    return TRUE;
}

/* drop all threads (except for the currently active one) */
void ht_scheduler_drop(void)
{
    ht_t t;

    /* clear the new queue */
    while ((t = ht_pqueue_delmax(&ht_NQ)) != NULL)
        ht_tcb_free(t);
    ht_pqueue_init(&ht_NQ);

    /* clear the ready queue */
    while ((t = ht_pqueue_delmax(&ht_RQ)) != NULL)
        ht_tcb_free(t);
    ht_pqueue_init(&ht_RQ);

    /* clear the waiting queue */
    while ((t = ht_pqueue_delmax(&ht_WQ)) != NULL)
        ht_tcb_free(t);
    ht_pqueue_init(&ht_WQ);

    /* clear the suspend queue */
    while ((t = ht_pqueue_delmax(&ht_SQ)) != NULL)
        ht_tcb_free(t);
    ht_pqueue_init(&ht_SQ);

    /* clear the dead queue */
    while ((t = ht_pqueue_delmax(&ht_DQ)) != NULL)
        ht_tcb_free(t);
    ht_pqueue_init(&ht_DQ);
    return;
}

/* kill the scheduler ingredients */
void ht_scheduler_kill(void)
{
    /* drop all threads */
    ht_scheduler_drop();

    return;
}

/*
 * Update the average scheduler load.
 *
 * This is called on every context switch, but we have to adjust the
 * average load value every second, only. If we're called more than
 * once per second we handle this by just calculating anything once
 * and then do NOPs until the next ticks is over. If the scheduler
 * waited for more than once second (or a thread CPU burst lasted for
 * more than once second) we simulate the missing calculations. That's
 * no problem because we can assume that the number of ready threads
 * then wasn't changed dramatically (or more context switched would have
 * been occurred and we would have been given more chances to operate).
 * The actual average load is calculated through an exponential average
 * formula.
 */
#define ht_scheduler_load(now) \
    if (ht_time_cmp((now), &ht_loadticknext) >= 0) { \
        ht_time_t ttmp; \
        int numready; \
        numready = ht_pqueue_elements(&ht_RQ); \
        ht_time_set(&ttmp, (now)); \
        do { \
            ht_loadval = (numready*0.25) + (ht_loadval*0.75); \
            ht_time_sub(&ttmp, &ht_loadtickgap); \
        } while (ht_time_cmp(&ttmp, &ht_loadticknext) >= 0); \
        ht_time_set(&ht_loadticknext, (now)); \
        ht_time_add(&ht_loadticknext, &ht_loadtickgap); \
    }

/* the heart of this library: the thread scheduler */
void *ht_scheduler(void *dummy)
{
    ht_time_t running;
    ht_time_t snapshot;
    ht_t t;

    /*
     * bootstrapping
     */
    ht_debug1("ht_scheduler: bootstrapping");

    /* mark this thread as the special scheduler thread */
    ht_sched->state = HT_STATE_SCHEDULER;

    /* initialize the snapshot time for bootstrapping the loop */
    ht_time_set(&snapshot, HT_TIME_NOW);

    /*
     * endless scheduler loop
     */
    for (;;) {
        /*
         * Move threads from new queue to ready queue and optionally
         * give them maximum priority so they start immediately.
         */
        while ((t = ht_pqueue_tail(&ht_NQ)) != NULL) {
            ht_pqueue_delete(&ht_NQ, t);
            t->state = HT_STATE_READY;
            if (ht_favournew)
                ht_pqueue_insert(&ht_RQ, ht_pqueue_favorite_prio(&ht_RQ), t);
            else
                ht_pqueue_insert(&ht_RQ, HT_PRIO_STD, t);
            ht_debug2("ht_scheduler: new thread \"%s\" moved to top of ready queue", t->name);
        }

        /*
         * Update average scheduler load
         */
        ht_scheduler_load(&snapshot);

        /*
         * Find next thread in ready queue
         */
        ht_current = ht_pqueue_delmax(&ht_RQ);
        if (ht_current == NULL) {
            fprintf(stderr, "**Pth** SCHEDULER INTERNAL ERROR: "
                            "no more thread(s) available to schedule!?!?\n");
            abort();
        }
        ht_debug4("ht_scheduler: thread \"%s\" selected (prio=%d, qprio=%d)",
                   ht_current->name, ht_current->prio, ht_current->q_prio);

        /*
         * Set running start time for new thread
         * and perform a context switch to it
         */
        ht_debug3("ht_scheduler: switching to thread 0x%lx (\"%s\")",
                   (unsigned long)ht_current, ht_current->name);

        /* update thread times */
        ht_time_set(&ht_current->lastran, HT_TIME_NOW);

        /* update scheduler times */
        ht_time_set(&running, &ht_current->lastran);
        ht_time_sub(&running, &snapshot);
        ht_time_add(&ht_sched->running, &running);

        /* ** ENTERING THREAD ** - by switching the machine context */
        ht_current->dispatches++;
        ht_mctx_switch(&ht_sched->mctx, &ht_current->mctx);

        /* update scheduler times */
        ht_time_set(&snapshot, HT_TIME_NOW);
        ht_debug3("ht_scheduler: cameback from thread 0x%lx (\"%s\")",
                   (unsigned long)ht_current, ht_current->name);

        /*
         * Calculate and update the time the previous thread was running
         */
        ht_time_set(&running, &snapshot);
        ht_time_sub(&running, &ht_current->lastran);
        ht_time_add(&ht_current->running, &running);
        ht_debug3("ht_scheduler: thread \"%s\" ran %.6f",
                   ht_current->name, ht_time_t2d(&running));

        /*
         * Check for stack overflow
         */
        if (ht_current->stackguard != NULL) {
            if (*ht_current->stackguard != 0xDEAD) {
                ht_debug3("ht_scheduler: stack overflow detected for thread 0x%lx (\"%s\")",
                           (unsigned long)ht_current, ht_current->name);
                /*
                 * if the application doesn't catch SIGSEGVs, we terminate
                 * manually with a SIGSEGV now, but output a reasonable message.
                 */
                struct sigaction sa;
                sigset_t ss;
                if (sigaction(SIGSEGV, NULL, &sa) == 0) {
                    if (sa.sa_handler == SIG_DFL) {
                        fprintf(stderr, "**Pth** STACK OVERFLOW: thread pid_t=0x%lx, name=\"%s\"\n",
                                (unsigned long)ht_current, ht_current->name);
                        kill(getpid(), SIGSEGV);
                        sigfillset(&ss);
                        sigdelset(&ss, SIGSEGV);
                        sigsuspend(&ss);
                        abort();
                    }
                }
                /*
                 * else we terminate the thread only and send us a SIGSEGV
                 * which allows the application to handle the situation...
                 */
                ht_current->join_arg = (void *)0xDEAD;
                ht_current->state = HT_STATE_DEAD;
                kill(getpid(), SIGSEGV);
            }
        }

        /*
         * If previous thread is now marked as dead, kick it out
         */
        if (ht_current->state == HT_STATE_DEAD) {
            ht_debug2("ht_scheduler: marking thread \"%s\" as dead", ht_current->name);
            if (!ht_current->joinable)
                ht_tcb_free(ht_current);
            else
                ht_pqueue_insert(&ht_DQ, HT_PRIO_STD, ht_current);
            ht_current = NULL;
        }

        /*
         * If thread wants to wait for an event
         * move it to waiting queue now
         */
        if (ht_current != NULL && ht_current->state == HT_STATE_WAITING) {
            ht_debug2("ht_scheduler: moving thread \"%s\" to waiting queue",
                       ht_current->name);
            ht_pqueue_insert(&ht_WQ, ht_current->prio, ht_current);
            ht_current = NULL;
        }

        /*
         * migrate old treads in ready queue into higher
         * priorities to avoid starvation and insert last running
         * thread back into this queue, too.
         */
        ht_pqueue_increase(&ht_RQ);
        if (ht_current != NULL)
            ht_pqueue_insert(&ht_RQ, ht_current->prio, ht_current);

        /*
         * Manage the events in the waiting queue, i.e. decide whether their
         * events occurred and move them to the ready queue. But wait only if
         * we have already no new or ready threads.
         */
        if (   ht_pqueue_elements(&ht_RQ) == 0
            && ht_pqueue_elements(&ht_NQ) == 0)
            /* still no NEW or READY threads, so we have to wait for new work */
            ht_sched_eventmanager(&snapshot, FALSE /* wait */);
        else
            /* already NEW or READY threads exists, so just poll for even more work */
            ht_sched_eventmanager(&snapshot, TRUE  /* poll */);
    }

    /* NOTREACHED */
    return NULL;
}

/*
 * Look whether some events already occurred (or failed) and move
 * corresponding threads from waiting queue back to ready queue.
 */
void ht_sched_eventmanager(ht_time_t *now, int dopoll)
{
    ht_t nexttimer_thread;
    ht_event_t nexttimer_ev;
    ht_time_t nexttimer_value;
    ht_event_t evh;
    ht_event_t ev;
    ht_t t;
    ht_t tlast;
    int this_occurred;
    int any_occurred;
    fd_set rfds;
    fd_set wfds;
    fd_set efds;
    struct timeval delay;
    struct timeval *pdelay;
    int loop_repeat;
    int fdmax;
    int rc;
    int n;

    ht_debug2("ht_sched_eventmanager: enter in %s mode",
               dopoll ? "polling" : "waiting");

    /* entry point for internal looping in event handling */
    loop_entry:
    loop_repeat = FALSE;

    /* initialize fd sets */
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);
    fdmax = -1;

    /* initialize next timer */
    ht_time_set(&nexttimer_value, HT_TIME_ZERO);
    nexttimer_thread = NULL;
    nexttimer_ev = NULL;

    /* for all threads in the waiting queue... */
    any_occurred = FALSE;
    for (t = ht_pqueue_head(&ht_WQ); t != NULL;
         t = ht_pqueue_walk(&ht_WQ, t, HT_WALK_NEXT)) {

        /* cancellation support */
        if (t->cancelreq == TRUE)
            any_occurred = TRUE;

        /* ... and all their events... */
        if (t->events == NULL)
            continue;
        /* ...check whether events occurred */
        ev = evh = t->events;
        do {
            if (ev->ev_status == HT_STATUS_PENDING) {
                this_occurred = FALSE;

                /* Filedescriptor I/O */
                if (ev->ev_type == HT_EVENT_FD) {
                    /* filedescriptors are checked later all at once.
                       Here we only assemble them in the fd sets */
                    if (ev->ev_goal & HT_UNTIL_FD_READABLE)
                        FD_SET(ev->ev_args.FD.fd, &rfds);
                    if (ev->ev_goal & HT_UNTIL_FD_WRITEABLE)
                        FD_SET(ev->ev_args.FD.fd, &wfds);
                    if (ev->ev_goal & HT_UNTIL_FD_EXCEPTION)
                        FD_SET(ev->ev_args.FD.fd, &efds);
                    if (fdmax < ev->ev_args.FD.fd)
                        fdmax = ev->ev_args.FD.fd;
                }
                /* Filedescriptor Set Select I/O */
                else if (ev->ev_type == HT_EVENT_SELECT) {
                    /* filedescriptors are checked later all at once.
                       Here we only merge the fd sets. */
                    ht_util_fds_merge(ev->ev_args.SELECT.nfd,
                                       ev->ev_args.SELECT.rfds, &rfds,
                                       ev->ev_args.SELECT.wfds, &wfds,
                                       ev->ev_args.SELECT.efds, &efds);
                    if (fdmax < ev->ev_args.SELECT.nfd-1)
                        fdmax = ev->ev_args.SELECT.nfd-1;
                }
                /* Timer */
                else if (ev->ev_type == HT_EVENT_TIME) {
                    if (ht_time_cmp(&(ev->ev_args.TIME.tv), now) < 0)
                        this_occurred = TRUE;
                    else {
                        /* remember the timer which will be elapsed next */
                        if ((nexttimer_thread == NULL && nexttimer_ev == NULL) ||
                            ht_time_cmp(&(ev->ev_args.TIME.tv), &nexttimer_value) < 0) {
                            nexttimer_thread = t;
                            nexttimer_ev = ev;
                            ht_time_set(&nexttimer_value, &(ev->ev_args.TIME.tv));
                        }
                    }
                }
                /* Message Port Arrivals */
                else if (ev->ev_type == HT_EVENT_MSG) {
                    if (ht_ring_elements(&(ev->ev_args.MSG.mp->mp_queue)) > 0)
                        this_occurred = TRUE;
                }
                /* Mutex Release */
                else if (ev->ev_type == HT_EVENT_MUTEX) {
                    if (!(ev->ev_args.MUTEX.mutex->mx_state & HT_MUTEX_LOCKED))
                        this_occurred = TRUE;
                }
                /* Condition Variable Signal */
                else if (ev->ev_type == HT_EVENT_COND) {
                    if (ev->ev_args.COND.cond->cn_state & HT_COND_SIGNALED) {
                        if (ev->ev_args.COND.cond->cn_state & HT_COND_BROADCAST)
                            this_occurred = TRUE;
                        else {
                            if (!(ev->ev_args.COND.cond->cn_state & HT_COND_HANDLED)) {
                                ev->ev_args.COND.cond->cn_state |= HT_COND_HANDLED;
                                this_occurred = TRUE;
                            }
                        }
                    }
                }
                /* Thread Termination */
                else if (ev->ev_type == HT_EVENT_TID) {
                    if (   (   ev->ev_args.TID.tid == NULL
                            && ht_pqueue_elements(&ht_DQ) > 0)
                        || (   ev->ev_args.TID.tid != NULL
                            && ev->ev_args.TID.tid->state == ev->ev_goal))
                        this_occurred = TRUE;
                }
                /* Custom Event Function */
                else if (ev->ev_type == HT_EVENT_FUNC) {
                    if (ev->ev_args.FUNC.func(ev->ev_args.FUNC.arg))
                        this_occurred = TRUE;
                    else {
                        ht_time_t tv;
                        ht_time_set(&tv, now);
                        ht_time_add(&tv, &(ev->ev_args.FUNC.tv));
                        if ((nexttimer_thread == NULL && nexttimer_ev == NULL) ||
                            ht_time_cmp(&tv, &nexttimer_value) < 0) {
                            nexttimer_thread = t;
                            nexttimer_ev = ev;
                            ht_time_set(&nexttimer_value, &tv);
                        }
                    }
                }

                /* tag event if it has occurred */
                if (this_occurred) {
                    ht_debug2("ht_sched_eventmanager: [non-I/O] event occurred for thread \"%s\"", t->name);
                    ev->ev_status = HT_STATUS_OCCURRED;
                    any_occurred = TRUE;
                }
            }
        } while ((ev = ev->ev_next) != evh);
    }
    if (any_occurred)
        dopoll = TRUE;

    /* now decide how to poll for fd I/O and timers */
    if (dopoll) {
        /* do a polling with immediate timeout,
           i.e. check the fd sets only without blocking */
        ht_time_set(&delay, HT_TIME_ZERO);
        pdelay = &delay;
    }
    else if (nexttimer_ev != NULL) {
        /* do a polling with a timeout set to the next timer,
           i.e. wait for the fd sets or the next timer */
        ht_time_set(&delay, &nexttimer_value);
        ht_time_sub(&delay, now);
        pdelay = &delay;
    }
    else {
        /* do a polling without a timeout,
           i.e. wait for the fd sets only with blocking */
        pdelay = NULL;
    }

    /* now do the polling for filedescriptor I/O and timers
       WHEN THE SCHEDULER SLEEPS AT ALL, THEN HERE!! */
    rc = -1;
    if (!(dopoll && fdmax == -1))
        while ((rc = select(fdmax+1, &rfds, &wfds, &efds, pdelay)) < 0
               && errno == EINTR) ;

    /* if the timer elapsed, handle it */
    if (!dopoll && rc == 0 && nexttimer_ev != NULL) {
        if (nexttimer_ev->ev_type == HT_EVENT_FUNC) {
            /* it was an implicit timer event for a function event,
               so repeat the event handling for rechecking the function */
            loop_repeat = TRUE;
        }
        else {
            /* it was an explicit timer event, standing for its own */
            ht_debug2("ht_sched_eventmanager: [timeout] event occurred for thread \"%s\"",
                       nexttimer_thread->name);
            nexttimer_ev->ev_status = HT_STATUS_OCCURRED;
        }
    }

    /* if an error occurred, avoid confusion in the cleanup loop */
    if (rc <= 0) {
        FD_ZERO(&rfds);
        FD_ZERO(&wfds);
        FD_ZERO(&efds);
    }

    /* now comes the final cleanup loop where we've to
       do two jobs: first we've to do the late handling of the fd I/O events and
       additionally if a thread has one occurred event, we move it from the
       waiting queue to the ready queue */

    /* for all threads in the waiting queue... */
    t = ht_pqueue_head(&ht_WQ);
    while (t != NULL) {

        /* do the late handling of the fd I/O and signal
           events in the waiting event ring */
        any_occurred = FALSE;
        if (t->events != NULL) {
            ev = evh = t->events;
            do {
                /*
                 * Late handling for still not occured events
                 */
                if (ev->ev_status == HT_STATUS_PENDING) {
                    /* Filedescriptor I/O */
                    if (ev->ev_type == HT_EVENT_FD) {
                        if (   (   ev->ev_goal & HT_UNTIL_FD_READABLE
                                && FD_ISSET(ev->ev_args.FD.fd, &rfds))
                            || (   ev->ev_goal & HT_UNTIL_FD_WRITEABLE
                                && FD_ISSET(ev->ev_args.FD.fd, &wfds))
                            || (   ev->ev_goal & HT_UNTIL_FD_EXCEPTION
                                && FD_ISSET(ev->ev_args.FD.fd, &efds)) ) {
                            ht_debug2("ht_sched_eventmanager: "
                                       "[I/O] event occurred for thread \"%s\"", t->name);
                            ev->ev_status = HT_STATUS_OCCURRED;
                        }
                        else if (rc < 0) {
                            /* re-check particular filedescriptor */
                            int rc2;
                            if (ev->ev_goal & HT_UNTIL_FD_READABLE)
                                FD_SET(ev->ev_args.FD.fd, &rfds);
                            if (ev->ev_goal & HT_UNTIL_FD_WRITEABLE)
                                FD_SET(ev->ev_args.FD.fd, &wfds);
                            if (ev->ev_goal & HT_UNTIL_FD_EXCEPTION)
                                FD_SET(ev->ev_args.FD.fd, &efds);
                            ht_time_set(&delay, HT_TIME_ZERO);
                            while ((rc2 = select(ev->ev_args.FD.fd+1, &rfds, &wfds, &efds, &delay)) < 0
                                   && errno == EINTR) ;
                            if (rc2 > 0) {
                                /* cleanup afterwards for next iteration */
                                FD_CLR(ev->ev_args.FD.fd, &rfds);
                                FD_CLR(ev->ev_args.FD.fd, &wfds);
                                FD_CLR(ev->ev_args.FD.fd, &efds);
                            } else if (rc2 < 0) {
                                /* cleanup afterwards for next iteration */
                                FD_ZERO(&rfds);
                                FD_ZERO(&wfds);
                                FD_ZERO(&efds);
                                ev->ev_status = HT_STATUS_FAILED;
                                ht_debug2("ht_sched_eventmanager: "
                                           "[I/O] event failed for thread \"%s\"", t->name);
                            }
                        }
                    }
                    /* Filedescriptor Set I/O */
                    else if (ev->ev_type == HT_EVENT_SELECT) {
                        if (ht_util_fds_test(ev->ev_args.SELECT.nfd,
                                              ev->ev_args.SELECT.rfds, &rfds,
                                              ev->ev_args.SELECT.wfds, &wfds,
                                              ev->ev_args.SELECT.efds, &efds)) {
                            n = ht_util_fds_select(ev->ev_args.SELECT.nfd,
                                                    ev->ev_args.SELECT.rfds, &rfds,
                                                    ev->ev_args.SELECT.wfds, &wfds,
                                                    ev->ev_args.SELECT.efds, &efds);
                            if (ev->ev_args.SELECT.n != NULL)
                                *(ev->ev_args.SELECT.n) = n;
                            ev->ev_status = HT_STATUS_OCCURRED;
                            ht_debug2("ht_sched_eventmanager: "
                                       "[I/O] event occurred for thread \"%s\"", t->name);
                        }
                        else if (rc < 0) {
                            /* re-check particular filedescriptor set */
                            int rc2;
                            fd_set *prfds = NULL;
                            fd_set *pwfds = NULL;
                            fd_set *pefds = NULL;
                            fd_set trfds;
                            fd_set twfds;
                            fd_set tefds;
                            if (ev->ev_args.SELECT.rfds) {
                                memcpy(&trfds, ev->ev_args.SELECT.rfds, sizeof(rfds));
                                prfds = &trfds;
                            }
                            if (ev->ev_args.SELECT.wfds) {
                                memcpy(&twfds, ev->ev_args.SELECT.wfds, sizeof(wfds));
                                pwfds = &twfds;
                            }
                            if (ev->ev_args.SELECT.efds) {
                                memcpy(&tefds, ev->ev_args.SELECT.efds, sizeof(efds));
                                pefds = &tefds;
                            }
                            ht_time_set(&delay, HT_TIME_ZERO);
                            while ((rc2 = select(ev->ev_args.SELECT.nfd+1, prfds, pwfds, pefds, &delay)) < 0
                                   && errno == EINTR) ;
                            if (rc2 < 0) {
                                ev->ev_status = HT_STATUS_FAILED;
                                ht_debug2("ht_sched_eventmanager: "
                                           "[I/O] event failed for thread \"%s\"", t->name);
                            }
                        }
                    }
                }
                /*
                 * post-processing for already occured events
                 */
                else {
                    /* Condition Variable Signal */
                    if (ev->ev_type == HT_EVENT_COND) {
                        /* clean signal */
                        if (ev->ev_args.COND.cond->cn_state & HT_COND_SIGNALED) {
                            ev->ev_args.COND.cond->cn_state &= ~(HT_COND_SIGNALED);
                            ev->ev_args.COND.cond->cn_state &= ~(HT_COND_BROADCAST);
                            ev->ev_args.COND.cond->cn_state &= ~(HT_COND_HANDLED);
                        }
                    }
                }

                /* local to global mapping */
                if (ev->ev_status != HT_STATUS_PENDING)
                    any_occurred = TRUE;
            } while ((ev = ev->ev_next) != evh);
        }

        /* cancellation support */
        if (t->cancelreq == TRUE) {
            ht_debug2("ht_sched_eventmanager: cancellation request pending for thread \"%s\"", t->name);
            any_occurred = TRUE;
        }

        /* walk to next thread in waiting queue */
        tlast = t;
        t = ht_pqueue_walk(&ht_WQ, t, HT_WALK_NEXT);

        /*
         * move last thread to ready queue if any events occurred for it.
         * we insert it with a slightly increased queue priority to it a
         * better chance to immediately get scheduled, else the last running
         * thread might immediately get again the CPU which is usually not
         * what we want, because we oven use ht_yield() calls to give others
         * a chance.
         */
        if (any_occurred) {
            ht_pqueue_delete(&ht_WQ, tlast);
            tlast->state = HT_STATE_READY;
            ht_pqueue_insert(&ht_RQ, tlast->prio+1, tlast);
            ht_debug2("ht_sched_eventmanager: thread \"%s\" moved from waiting "
                       "to ready queue", tlast->name);
        }
    }

    /* perhaps we have to internally loop... */
    if (loop_repeat) {
        ht_time_set(now, HT_TIME_NOW);
        goto loop_entry;
    }

    ht_debug1("ht_sched_eventmanager: leaving");
    return;
}

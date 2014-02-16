/*
**  ht_p.h: Pth private API definitions
*/

#ifndef _HT_P_H_
#define _HT_P_H_

/* mandatory system headers */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <time.h>
#include <ucontext.h>

/* public API headers */
#include "ht.h"

/* dmalloc support */
#ifdef HT_DMALLOC
#include <dmalloc.h>
#endif

/* non-blocking flags */
#ifdef  O_NONBLOCK
#define O_NONBLOCKING O_NONBLOCK
#else
#ifdef  O_NDELAY
#define O_NONBLOCKING O_NDELAY
#else
#ifdef  FNDELAY
#define O_NONBLOCKING FNDELAY
#else
#error "No O_NONBLOCK, O_NDELAY or FNDELAY flag available!"
#endif
#endif
#endif

/* fallback definition for fdset_t size */
#if !defined(FD_SETSIZE)
#define FD_SETSIZE 1024
#endif

/* compiler happyness: avoid ``empty compilation unit'' problem */
#define COMPILER_HAPPYNESS(name) \
    int __##name##_unit = 0;

/* ht_mctx.c */
typedef struct ht_mctx_st ht_mctx_t;
struct ht_mctx_st {
    ucontext_t uc;
    int restored;
    int error;
};
/*
** ____ MACHINE STATE SWITCHING ______________________________________
*/

/*
 * save the current machine context
 */
#define ht_mctx_save(mctx) \
        ( (mctx)->error = errno, \
          (mctx)->restored = 0, \
          getcontext(&(mctx)->uc), \
          (mctx)->restored )
/*
 * restore the current machine context
 * (at the location of the old context)
 */
#define ht_mctx_restore(mctx) \
        ( errno = (mctx)->error, \
          (mctx)->restored = 1, \
          (void)setcontext(&(mctx)->uc) )
/*
 * switch from one machine context to another
 */
#define SWITCH_DEBUG_LINE \
        "==== THREAD CONTEXT SWITCH ==========================================="
#ifdef HT_DEBUG
#define  _ht_mctx_switch_debug ht_debug(NULL, 0, 1, SWITCH_DEBUG_LINE);
#else
#define  _ht_mctx_switch_debug /*NOP*/
#endif /*HT_DEBUG*/
#define ht_mctx_switch(old,new) \
    _ht_mctx_switch_debug \
    swapcontext(&((old)->uc), &((new)->uc));
extern int ht_mctx_set(ht_mctx_t *, void (*)(void), char *, char *);

/* ht_clean.c */
typedef struct ht_cleanup_st ht_cleanup_t;
struct ht_cleanup_st {
    ht_cleanup_t *next;
    void (*func)(void *);
    void *arg;
};
extern void ht_cleanup_popall(ht_t, int);
/* ht_tcb.c */
#define HT_TCB_NAMELEN 40
    /* thread control block */
struct ht_st {
   /* priority queue handling */
   ht_t           q_next;               /* next thread in pool                         */
   ht_t           q_prev;               /* previous thread in pool                     */
   int            q_prio;               /* (relative) priority of thread when queued   */

   /* standard thread control block ingredients */
   int            prio;                 /* base priority of thread                     */
   char           name[HT_TCB_NAMELEN];/* name of thread (mainly for debugging)       */
   int            dispatches;           /* total number of thread dispatches           */
   ht_state_t     state;                /* current state indicator for thread          */

   /* timing */
   ht_time_t      spawned;              /* time point at which thread was spawned      */
   ht_time_t      lastran;              /* time point at which thread was last running */
   ht_time_t      running;              /* time range the thread was already running   */

   /* event handling */
   ht_event_t     events;               /* events the tread is waiting for             */

   /* machine context */
   ht_mctx_t      mctx;                 /* last saved machine state of thread          */
   char           *stack;                /* pointer to thread stack                     */
   unsigned int   stacksize;            /* size of thread stack                        */
   long           *stackguard;           /* stack overflow guard                        */
   int            stackloan;            /* stack type                                  */
   void           *(*start_func)(void *);  /* start routine                               */
   void           *start_arg;            /* start argument                              */

   /* thread joining */
   int            joinable;             /* whether thread is joinable                  */
   void           *join_arg;             /* joining argument                            */

   /* per-thread specific storage */
   const void     **data_value;           /* thread specific  values                     */
   int            data_count;           /* number of stored values                     */

   /* cancellation support */
   int            cancelreq;            /* cancellation request is pending             */
   unsigned int   cancelstate;          /* cancellation state of thread                */
   ht_cleanup_t   *cleanups;             /* stack of thread cleanup handlers            */

   /* mutex ring */
   ht_ring_t      mutexring;            /* ring of aquired mutex structures            */
};
extern ht_t ht_tcb_alloc(unsigned int, void *);
extern void ht_tcb_free(ht_t);
/* ht_pqueue.c */
typedef struct ht_pqueue_st ht_pqueue_t;
struct ht_pqueue_st {
   ht_t		q_head;
   int      q_num;
};
/* determine priority required to favorite a thread; O(1) */
#define ht_pqueue_favorite_prio(q) \
    ((q)->q_head != NULL ? (q)->q_head->q_prio + 1 : HT_PRIO_MAX)
#define ht_pqueue_elements(q) \
    ((q) == NULL ? (-1) : (q)->q_num)
#define ht_pqueue_head(q) \
    ((q) == NULL ? NULL : (q)->q_head)
extern void ht_pqueue_init(ht_pqueue_t *);
extern void ht_pqueue_insert(ht_pqueue_t *, int, ht_t);
extern ht_t ht_pqueue_delmax(ht_pqueue_t *);
extern void ht_pqueue_delete(ht_pqueue_t *, ht_t);
extern int ht_pqueue_favorite(ht_pqueue_t *, ht_t);
extern void ht_pqueue_increase(ht_pqueue_t *);
extern ht_t ht_pqueue_tail(ht_pqueue_t *);
extern ht_t ht_pqueue_walk(ht_pqueue_t *, ht_t, int);
extern int ht_pqueue_contains(ht_pqueue_t *, ht_t);
/* ht_util.c */
#define ht_util_min(a,b) \
           ((a) > (b) ? (b) : (a))
extern char *ht_util_cpystrn(char *, const char *, size_t);
extern int ht_util_fd_valid(int);
extern void ht_util_fds_merge(int, fd_set *, fd_set *, fd_set *, fd_set *, fd_set *, fd_set *);
extern int ht_util_fds_test(int, fd_set *, fd_set *, fd_set *, fd_set *, fd_set *, fd_set *);
extern int ht_util_fds_select(int, fd_set *, fd_set *, fd_set *, fd_set *, fd_set *, fd_set *);
/* ht_sched.c  */
extern ht_t ht_main;
extern ht_t ht_sched;
extern ht_t ht_current;
extern ht_pqueue_t ht_NQ;
extern ht_pqueue_t ht_RQ;
extern ht_pqueue_t ht_WQ;
extern ht_pqueue_t ht_SQ;
extern ht_pqueue_t ht_DQ;
extern int ht_favournew;
extern float ht_loadval;
extern int ht_initialized;
extern int ht_scheduler_init(void);
extern void ht_scheduler_drop(void);
extern void ht_scheduler_kill(void);
extern void *ht_scheduler(void *);
extern void ht_sched_eventmanager(ht_time_t *, int);

/* ht_debug.c  */
#ifndef HT_DEBUG

#define ht_debug1(a1)                     /* NOP */
#define ht_debug2(a1, a2)                 /* NOP */
#define ht_debug3(a1, a2, a3)             /* NOP */
#define ht_debug4(a1, a2, a3, a4)         /* NOP */
#define ht_debug5(a1, a2, a3, a4, a5)     /* NOP */
#define ht_debug6(a1, a2, a3, a4, a5, a6) /* NOP */

#else

#define ht_debug1(a1)                     ht_debug(__FILE__, __LINE__, 1, a1)
#define ht_debug2(a1, a2)                 ht_debug(__FILE__, __LINE__, 2, a1, a2)
#define ht_debug3(a1, a2, a3)             ht_debug(__FILE__, __LINE__, 3, a1, a2, a3)
#define ht_debug4(a1, a2, a3, a4)         ht_debug(__FILE__, __LINE__, 4, a1, a2, a3, a4)
#define ht_debug5(a1, a2, a3, a4, a5)     ht_debug(__FILE__, __LINE__, 5, a1, a2, a3, a4, a5)
#define ht_debug6(a1, a2, a3, a4, a5, a6) ht_debug(__FILE__, __LINE__, 6, a1, a2, a3, a4, a5, a6)

#endif /* HT_DEBUG */
extern void ht_debug(const char *, int, int, const char *, ...);
extern void ht_dumpstate(FILE *);
extern void ht_dumpqueue(FILE *, const char *, ht_pqueue_t *);

/* ht_errno.c  */
/* enclose errno in a block */
#define ht_shield \
        for ( ht_errno_storage = errno, \
              ht_errno_flag = TRUE; \
              ht_errno_flag; \
              errno = ht_errno_storage, \
              ht_errno_flag = FALSE )

/* return plus setting an errno value */
#if defined(HT_DEBUG)
#define ht_error(return_val,errno_val) \
       (errno = (errno_val), \
       ht_debug4("return 0x%lx with errno %d(\"%s\")", \
                 (unsigned long)(return_val), (errno), strerror((errno))), \
       (return_val))
#else
#define ht_error(return_val,errno_val) \
       (errno = (errno_val), (return_val))
#endif
extern int ht_errno_storage;
extern int ht_errno_flag;
/* ht_string.c */
extern int ht_vsnprintf(char *, size_t, const char *, va_list);
extern int ht_snprintf(char *, size_t, const char *, ...);
extern char * ht_vasprintf(const char *, va_list);
extern char * ht_asprintf(const char *, ...);
/* ht_attr.c */
enum {
       HT_ATTR_GET,
       HT_ATTR_SET
};

struct ht_attr_st {
       ht_t         a_tid;
       int          a_prio;
       int          a_dispatches;
       char         a_name[HT_TCB_NAMELEN];
       int          a_joinable;
       unsigned int a_cancelstate;
       unsigned int a_stacksize;
       char        *a_stackaddr;
};
extern int ht_attr_ctrl(int, ht_attr_t, int, va_list);
/* ht_time.c */
#define HT_TIME_NOW  (ht_time_t *)(0)
#define HT_TIME_ZERO &ht_time_zero
#define HT_TIME(sec,usec) { sec, usec }
#define ht_time_equal(t1,t2) \
        (((t1).tv_sec == (t2).tv_sec) && ((t1).tv_usec == (t2).tv_usec))
#define ht_time_set(t1,t2) \
    do { \
        if ((t2) == HT_TIME_NOW) \
            gettimeofday((t1), NULL); \
        else { \
            (t1)->tv_sec  = (t2)->tv_sec; \
            (t1)->tv_usec = (t2)->tv_usec; \
        } \
    } while (0)
#define ht_time_add(t1,t2) \
    (t1)->tv_sec  += (t2)->tv_sec; \
    (t1)->tv_usec += (t2)->tv_usec; \
    if ((t1)->tv_usec > 1000000) { \
        (t1)->tv_sec  += 1; \
        (t1)->tv_usec -= 1000000; \
    }
#define ht_time_sub(t1,t2) \
    (t1)->tv_sec  -= (t2)->tv_sec; \
    (t1)->tv_usec -= (t2)->tv_usec; \
    if ((t1)->tv_usec < 0) { \
        (t1)->tv_sec  -= 1; \
        (t1)->tv_usec += 1000000; \
    }
extern void ht_time_usleep(unsigned long);
extern int ht_time_cmp(ht_time_t *, ht_time_t *);
extern void ht_time_div(ht_time_t *, int);
extern void ht_time_mul(ht_time_t *, int);
extern double ht_time_t2d(ht_time_t *);
extern int ht_time_t2i(ht_time_t *);
extern int ht_time_pos(ht_time_t *);
extern ht_time_t ht_time_zero;
/* ht_msg.c */
struct ht_msgport_st {
    ht_ringnode_t mp_node;  /* maintainance node handle */
    const char    *mp_name;  /* optional name of message port */
    ht_t          mp_tid;   /* corresponding thread */
    ht_ring_t     mp_queue; /* queue of messages pending on port */
};
/* ht_event.c */
typedef int (*ht_event_func_t)(void *);
struct ht_event_st {
    struct ht_event_st *ev_next;
    struct ht_event_st *ev_prev;
    ht_status_t ev_status;
    int ev_type;
    int ev_goal;
    union {
        struct { int fd; }                                          FD;
        struct { int *n; int nfd; fd_set *rfds, *wfds, *efds; }     SELECT;
        struct { ht_time_t tv; }                                    TIME;
        struct { ht_msgport_t mp; }                                 MSG;
        struct { ht_mutex_t *mutex; }                               MUTEX;
        struct { ht_cond_t *cond; }                                 COND;
        struct { ht_t tid; }                                        TID;
        struct { ht_event_func_t func; void *arg; ht_time_t tv; }   FUNC;
    } ev_args;
};
/* ht_ring.c */
/* return number of nodes in ring; O(1) */
#define ht_ring_elements(r) \
    ((r) == NULL ? (-1) : (r)->r_nodes)
/* return first node in ring; O(1) */
#define ht_ring_first(r) \
    ((r) == NULL ? NULL : (r)->r_hook)
/* return last node in ring; O(1) */
#define ht_ring_last(r) \
    ((r) == NULL ? NULL : ((r)->r_hook == NULL ? NULL : (r)->r_hook->rn_prev))
/* walk to next node in ring; O(1) */
#define ht_ring_next(r, rn) \
    (((r) == NULL || (rn) == NULL) ? NULL : ((rn)->rn_next == (r)->r_hook ? NULL : (rn)->rn_next))
/* walk to previous node in ring; O(1) */
#define ht_ring_prev(r, rn) \
    (((r) == NULL || (rn) == NULL) ? NULL : ((rn)->rn_prev == (r)->r_hook->rn_prev ? NULL : (rn)->rn_prev))
/* insert node into ring; O(1) */
#define ht_ring_insert(r, rn) \
    ht_ring_append((r), (rn))
/* treat ring as stack: push node onto stack; O(1) */
#define ht_ring_push(r, rn) \
    ht_ring_prepend((r), (rn))
/* treat ring as queue: enqueue node; O(1) */
#define ht_ring_enqueue(r, rn) \
    ht_ring_prepend((r), (rn))
extern void ht_ring_init(ht_ring_t *);
extern void ht_ring_insert_after(ht_ring_t *, ht_ringnode_t *, ht_ringnode_t *);
extern void ht_ring_insert_before(ht_ring_t *, ht_ringnode_t *, ht_ringnode_t *);
extern void ht_ring_delete(ht_ring_t *, ht_ringnode_t *);
extern void ht_ring_prepend(ht_ring_t *, ht_ringnode_t *);
extern void ht_ring_append(ht_ring_t *, ht_ringnode_t *);
extern ht_ringnode_t *ht_ring_pop(ht_ring_t *);
extern int ht_ring_favorite(ht_ring_t *, ht_ringnode_t *);
extern ht_ringnode_t *ht_ring_dequeue(ht_ring_t *);
extern int ht_ring_contains(ht_ring_t *, ht_ringnode_t *);
/* ht_lib.c */
#define ht_implicit_init() \
       if (!ht_initialized) \
        ht_init();
extern int ht_initialized;
extern int ht_thread_exists(ht_t);
extern void ht_thread_cleanup(ht_t);
/* ht_high.c */
extern ssize_t ht_readv_faked(int, const struct iovec *, int);
extern ssize_t ht_writev_iov_bytes(const struct iovec *, int);
extern void ht_writev_iov_advance(const struct iovec *, int, size_t, struct iovec **, int *, struct iovec *, int);
extern ssize_t ht_writev_faked(int, const struct iovec *, int);
/* ht_data.c */
extern void ht_key_destroydata(ht_t);
/* ht_sync.c */
extern void ht_mutex_releaseall(ht_t);

#endif /* _HT_P_H_ */

#ifndef _HT_H_
#define _HT_H_

/* the library version */
#ifndef HT_VERSION_STR
#define HT_VERSION_STR "2.0.7 (08-Jun-2006)"
#endif
#ifndef HT_VERSION_HEX
#define HT_VERSION_HEX 0x200207
#endif
#ifndef HT_VERSION
#define HT_VERSION HT_VERSION_HEX
#endif

    /* essential headers */
#include <sys/types.h>     /* for ssize_t, off_t  */
#include <time.h>          /* for struct timespec */
#include <sys/time.h>      /* for struct timeval  */
#include <sys/socket.h>    /* for sockaddr        */
#include <sys/select.h>

    /* fallbacks for essential typedefs */
#ifndef _PTHREAD_PRIVATE
/* typedef int pid_t; */
/* typedef unsigned int size_t; */
/* typedef unsigned int ssize_t; */
/* typedef socklen_t socklen_t; */
/* typedef int off_t; */
/* typedef int sig_atomic_t; */
/* typedef nfds_t nfds_t; */
#endif /* !_PTHREAD_PRIVATE */

    /* extra structure definitions */
struct timeval;
struct timespec;

    /* essential values */
#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif
#ifndef NUL
#define NUL '\0'
#endif
#ifndef NULL
#define NULL (void *)0
#endif

    /* bitmask generation */
#define _BIT(n) (1<<(n))

    /* C++ support */
#ifdef __cplusplus
#define BEGIN_DECLARATION extern "C" {
#define END_DECLARATION   }
#else
#define BEGIN_DECLARATION /*nop*/
#define END_DECLARATION   /*nop*/
#endif

    /* check if the user requests a bigger FD_SETSIZE than we can handle */
#if defined(FD_SETSIZE)
#if FD_SETSIZE > 1024
#error "FD_SETSIZE is larger than what GNU Pth can handle."
#endif
#endif

BEGIN_DECLARATION

    /* some global constants */
#define HT_KEY_MAX                  256
#define HT_ATFORK_MAX               128
#define HT_DESTRUCTOR_ITERATIONS    4

    /* system call mapping support type (soft variant can be overridden) */
#define HT_SYSCALL_HARD 0
#ifndef HT_SYSCALL_SOFT
#define HT_SYSCALL_SOFT 0
#endif

    /* queries for ht_ctrl() */
#define HT_CTRL_GETAVLOAD            _BIT(1)
#define HT_CTRL_GETPRIO              _BIT(2)
#define HT_CTRL_GETNAME              _BIT(3)
#define HT_CTRL_GETTHREADS_NEW       _BIT(4)
#define HT_CTRL_GETTHREADS_READY     _BIT(5)
#define HT_CTRL_GETTHREADS_RUNNING   _BIT(6)
#define HT_CTRL_GETTHREADS_WAITING   _BIT(7)
#define HT_CTRL_GETTHREADS_SUSPENDED _BIT(8)
#define HT_CTRL_GETTHREADS_DEAD      _BIT(9)
#define HT_CTRL_GETTHREADS           (HT_CTRL_GETTHREADS_NEW|\
                                       HT_CTRL_GETTHREADS_READY|\
                                       HT_CTRL_GETTHREADS_RUNNING|\
                                       HT_CTRL_GETTHREADS_WAITING|\
                                       HT_CTRL_GETTHREADS_SUSPENDED|\
                                       HT_CTRL_GETTHREADS_DEAD)
#define HT_CTRL_DUMPSTATE            _BIT(10)
#define HT_CTRL_FAVOURNEW            _BIT(11)

    /* the time value structure */
typedef struct timeval ht_time_t;

    /* the unique thread id/handle */
typedef struct ht_st *ht_t;
struct ht_st;

    /* thread states */
typedef enum ht_state_en {
    HT_STATE_SCHEDULER = 0,         /* the special scheduler thread only       */
    HT_STATE_NEW,                   /* spawned, but still not dispatched       */
    HT_STATE_READY,                 /* ready, waiting to be dispatched         */
    HT_STATE_WAITING,               /* suspended, waiting until event occurred */
    HT_STATE_WAITING_FOR_SCHED_TO_WORKER,  /* a tempo state, scheduler send 
                                              thread to task queue and move it
                                              to HT_STATE_WAITING              */

    HT_STATE_DEAD                   /* terminated, waiting to be joined        */
} ht_state_t;

    /* thread priority values */
#define HT_PRIO_MAX                 +5
#define HT_PRIO_STD                  0
#define HT_PRIO_MIN                 -5

    /* the thread attribute structure */
typedef struct ht_attr_st *ht_attr_t;
struct ht_attr_st;

    /* attribute set/get commands for ht_attr_{get,set}() */
enum {
    HT_ATTR_PRIO,           /* RW [int]               priority of thread                */
    HT_ATTR_NAME,           /* RW [char *]            name of thread                    */
    HT_ATTR_JOINABLE,       /* RW [int]               thread detachment type            */
    HT_ATTR_CANCEL_STATE,   /* RW [unsigned int]      thread cancellation state         */
    HT_ATTR_STACK_SIZE,     /* RW [unsigned int]      stack size                        */
    HT_ATTR_STACK_ADDR,     /* RW [char *]            stack lower address               */
    HT_ATTR_DISPATCHES,     /* RO [int]               total number of thread dispatches */
    HT_ATTR_TIME_SPAWN,     /* RO [ht_time_t]        time thread was spawned           */
    HT_ATTR_TIME_LAST,      /* RO [ht_time_t]        time thread was last dispatched   */
    HT_ATTR_TIME_RAN,       /* RO [ht_time_t]        time thread was running           */
    HT_ATTR_START_FUNC,     /* RO [void *(*)(void *)] thread start function             */
    HT_ATTR_START_ARG,      /* RO [void *]            thread start argument             */
    HT_ATTR_STATE,          /* RO [ht_state_t]       scheduling state                  */
    HT_ATTR_EVENTS,         /* RO [ht_event_t]       events the thread is waiting for  */
    HT_ATTR_BOUND           /* RO [int]               whether object is bound to thread */
};

    /* default thread attribute */
#define HT_ATTR_DEFAULT (ht_attr_t)(0)

    /* the event structure */
typedef struct ht_event_st *ht_event_t;
struct ht_event_st;

    /* event subject classes */
#define HT_EVENT_FD                 _BIT(1)
#define HT_EVENT_SELECT             _BIT(2)
#define HT_EVENT_TASK               _BIT(3)
#define HT_EVENT_TIME               _BIT(4)
#define HT_EVENT_MSG                _BIT(5)
#define HT_EVENT_MUTEX              _BIT(6)
#define HT_EVENT_COND               _BIT(7)
#define HT_EVENT_TID                _BIT(8)
#define HT_EVENT_FUNC               _BIT(9)

    /* event occurange restrictions */
#define HT_UNTIL_OCCURRED           _BIT(11)
#define HT_UNTIL_FD_READABLE        _BIT(12)
#define HT_UNTIL_FD_WRITEABLE       _BIT(13)
#define HT_UNTIL_FD_EXCEPTION       _BIT(14)
#define HT_UNTIL_TID_NEW            _BIT(15)
#define HT_UNTIL_TID_READY          _BIT(16)
#define HT_UNTIL_TID_WAITING        _BIT(17)
#define HT_UNTIL_TID_DEAD           _BIT(18)

    /* event structure handling modes */
#define HT_MODE_REUSE               _BIT(20)
#define HT_MODE_CHAIN               _BIT(21)
#define HT_MODE_STATIC              _BIT(22)

    /* event deallocation types */
enum { HT_FREE_THIS, HT_FREE_ALL };

    /* event walking directions */
#define HT_WALK_NEXT                _BIT(1)
#define HT_WALK_PREV                _BIT(2)

    /* event status codes */
typedef enum {
    HT_STATUS_PENDING,
    HT_STATUS_OCCURRED,
    HT_STATUS_FAILED
} ht_status_t;

    /* the key type and init value */
typedef int ht_key_t;
#define HT_KEY_INIT (-1)

    /* the once structure and init value */
typedef int ht_once_t;
#define HT_ONCE_INIT FALSE

    /* general ring structure */
typedef struct ht_ringnode_st ht_ringnode_t;
struct ht_ringnode_st {
    ht_ringnode_t *rn_next;
    ht_ringnode_t *rn_prev;
};
typedef struct ht_ring_st ht_ring_t;
struct ht_ring_st {
    ht_ringnode_t *r_hook;
    unsigned int    r_nodes;
};
#define HT_RING_INIT { NULL }

    /* cancellation values */
#define HT_CANCEL_ENABLE            _BIT(0)
#define HT_CANCEL_DISABLE           _BIT(1)
#define HT_CANCEL_ASYNCHRONOUS      _BIT(2)
#define HT_CANCEL_DEFERRED          _BIT(3)
#define HT_CANCEL_DEFAULT           (HT_CANCEL_ENABLE|HT_CANCEL_DEFERRED)
#define HT_CANCELED                 ((void *)-1)

   /* mutex values */
#define HT_MUTEX_INITIALIZED        _BIT(0)
#define HT_MUTEX_LOCKED             _BIT(1)
#define HT_MUTEX_INIT               { {NULL, NULL}, HT_MUTEX_INITIALIZED, NULL, 0 }

   /* read-write lock values */
enum { HT_RWLOCK_RD, HT_RWLOCK_RW };
#define HT_RWLOCK_INITIALIZED       _BIT(0)
#define HT_RWLOCK_INIT              { HT_RWLOCK_INITIALIZED, HT_RWLOCK_RD, 0, \
                                       HT_MUTEX_INIT, HT_MUTEX_INIT }

   /* condition variable values */
#define HT_COND_INITIALIZED         _BIT(0)
#define HT_COND_SIGNALED            _BIT(1)
#define HT_COND_BROADCAST           _BIT(2)
#define HT_COND_HANDLED             _BIT(3)
#define HT_COND_INIT                { HT_COND_INITIALIZED, 0 }

   /* barrier variable values */
#define HT_BARRIER_INITIALIZED      _BIT(0)
#define HT_BARRIER_INIT(threshold)  { HT_BARRIER_INITIALIZED, \
                                       (threshold), (threshold), FALSE, \
                                       HT_COND_INIT, HT_MUTEX_INIT }
#define HT_BARRIER_HEADLIGHT        (-1)
#define HT_BARRIER_TAILLIGHT        (-2)

    /* the message port structure */
typedef struct ht_msgport_st *ht_msgport_t;
struct ht_msgport_st;

    /* the message structure */
typedef struct ht_message_st ht_message_t;
struct ht_message_st { /* not hidden to allow inclusion */
    ht_ringnode_t m_node;
    ht_msgport_t  m_replyport;
    unsigned int   m_size;
    void          *m_data;
};

    /* the mutex structure */
typedef struct ht_mutex_st ht_mutex_t;
struct ht_mutex_st { /* not hidden to avoid destructor */
    ht_ringnode_t mx_node;
    int            mx_state;
    ht_t          mx_owner;
    unsigned long  mx_count;
};

    /* the read-write lock structure */
typedef struct ht_rwlock_st ht_rwlock_t;
struct ht_rwlock_st { /* not hidden to avoid destructor */
    int            rw_state;
    unsigned int   rw_mode;
    unsigned long  rw_readers;
    ht_mutex_t    rw_mutex_rd;
    ht_mutex_t    rw_mutex_rw;
};

    /* the condition variable structure */
typedef struct ht_cond_st ht_cond_t;
struct ht_cond_st { /* not hidden to avoid destructor */
    unsigned long cn_state;
    unsigned int  cn_waiters;
};

    /* the barrier variable structure */
typedef struct ht_barrier_st ht_barrier_t;
struct ht_barrier_st { /* not hidden to avoid destructor */
    unsigned long br_state;
    int           br_threshold;
    int           br_count;
    int           br_cycle;
    ht_cond_t    br_cond;
    ht_mutex_t   br_mutex;
};

    /* the user-space context structure */
typedef struct ht_uctx_st *ht_uctx_t;
struct ht_uctx_st;

    /* filedescriptor blocking modes */
enum {
    HT_FDMODE_ERROR = -1,
    HT_FDMODE_POLL  =  0,
    HT_FDMODE_BLOCK,
    HT_FDMODE_NONBLOCK
};

    /* optionally fake poll(2) data structure and options */
#ifndef _PTHREAD_PRIVATE
#define HT_FAKE_POLL 0
#if !(HT_FAKE_POLL)
/* use vendor poll(2) environment */
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_set
#endif
#include <poll.h>
#ifdef _XOPEN_SOURCE_set
#undef _XOPEN_SOURCE_set
#undef _XOPEN_SOURCE
#endif
#ifndef POLLRDNORM
#define POLLRDNORM POLLIN
#endif
#ifndef POLLRDBAND
#define POLLRDBAND POLLIN
#endif
#ifndef POLLWRNORM
#define POLLWRNORM POLLOUT
#endif
#ifndef POLLWRBAND
#define POLLWRBAND POLLOUT
#endif
#ifndef INFTIM
#define INFTIM (-1)
#endif
#else
/* fake a poll(2) environment */
#define POLLIN      0x0001      /* any readable data available   */
#define POLLPRI     0x0002      /* OOB/Urgent readable data      */
#define POLLOUT     0x0004      /* file descriptor is writeable  */
#define POLLERR     0x0008      /* some poll error occurred      */
#define POLLHUP     0x0010      /* file descriptor was "hung up" */
#define POLLNVAL    0x0020      /* requested events "invalid"    */
#define POLLRDNORM  POLLIN
#define POLLRDBAND  POLLIN
#define POLLWRNORM  POLLOUT
#define POLLWRBAND  POLLOUT
#ifndef INFTIM
#define INFTIM      (-1)        /* poll infinite */
#endif
struct pollfd {
    int fd;                     /* which file descriptor to poll */
    short events;               /* events we are interested in   */
    short revents;              /* events found on return        */
};
#endif
#endif /* !_PTHREAD_PRIVATE */

    /* optionally fake readv(2)/writev(2) data structure and options */
#ifndef _PTHREAD_PRIVATE
#define HT_FAKE_RWV 0
#if !(HT_FAKE_RWV)
/* use vendor readv(2)/writev(2) environment */
#include <sys/uio.h>
#ifndef UIO_MAXIOV
#define UIO_MAXIOV 1024
#endif
#else
/* fake a readv(2)/writev(2) environment */
struct iovec {
    void  *iov_base;  /* memory base address */
    size_t iov_len;   /* memory chunk length */
};
#ifndef UIO_MAXIOV
#define UIO_MAXIOV 1024
#endif
#endif
#endif /* !_PTHREAD_PRIVATE */

typedef void *Sfdisc_t;

    /* global functions */
extern int            ht_init(void);
extern int            ht_kill(void);
extern long           ht_ctrl(unsigned long, ...);
extern long           ht_version(void);

    /* thread attribute functions */
extern ht_attr_t     ht_attr_of(ht_t);
extern ht_attr_t     ht_attr_new(void);
extern int            ht_attr_init(ht_attr_t);
extern int            ht_attr_set(ht_attr_t, int, ...);
extern int            ht_attr_get(ht_attr_t, int, ...);
extern int            ht_attr_destroy(ht_attr_t);

    /* thread functions */
extern ht_t          ht_spawn(ht_attr_t, void *(*)(void *), void *);
extern int            ht_once(ht_once_t *, void (*)(void *), void *);
extern ht_t          ht_self(void);
extern int            ht_suspend(ht_t);
extern int            ht_resume(ht_t);
extern int            ht_yield(ht_t);
extern int            ht_nap(ht_time_t);
extern int            ht_wait(ht_event_t);
extern int            ht_cancel(ht_t);
extern int            ht_abort(ht_t);
extern int            ht_join(ht_t, void **);
extern void           ht_exit(void *);

    /* utility functions */
extern int            ht_fdmode(int, int);
extern ht_time_t     ht_time(long, long);
extern ht_time_t     ht_timeout(long, long);

    /* cancellation functions */
extern void           ht_cancel_state(int, int *);
extern void           ht_cancel_point(void);

    /* event functions */
extern ht_event_t    ht_event(unsigned long, ...);
extern unsigned long  ht_event_typeof(ht_event_t);
extern int            ht_event_extract(ht_event_t ev, ...);
extern ht_event_t    ht_event_concat(ht_event_t, ...);
extern ht_event_t    ht_event_isolate(ht_event_t);
extern ht_event_t    ht_event_walk(ht_event_t, unsigned int);
extern ht_status_t   ht_event_status(ht_event_t);
extern int            ht_event_free(ht_event_t, int);

    /* key-based storage functions */
extern int            ht_key_create(ht_key_t *, void (*)(void *));
extern int            ht_key_delete(ht_key_t);
extern int            ht_key_setdata(ht_key_t, const void *);
extern void          *ht_key_getdata(ht_key_t);

    /* message port functions */
extern ht_msgport_t  ht_msgport_create(const char *);
extern void           ht_msgport_destroy(ht_msgport_t);
extern ht_msgport_t  ht_msgport_find(const char *);
extern int            ht_msgport_pending(ht_msgport_t);
extern int            ht_msgport_put(ht_msgport_t, ht_message_t *);
extern ht_message_t *ht_msgport_get(ht_msgport_t);
extern int            ht_msgport_reply(ht_message_t *);

    /* cleanup handler functions */
extern int            ht_cleanup_push(void (*)(void *), void *);
extern int            ht_cleanup_pop(int);

    /* synchronization functions */
extern int            ht_mutex_init(ht_mutex_t *);
extern int            ht_mutex_acquire(ht_mutex_t *, int, ht_event_t);
extern int            ht_mutex_release(ht_mutex_t *);
extern int            ht_rwlock_init(ht_rwlock_t *);
extern int            ht_rwlock_acquire(ht_rwlock_t *, int, int, ht_event_t);
extern int            ht_rwlock_release(ht_rwlock_t *);
extern int            ht_cond_init(ht_cond_t *);
extern int            ht_cond_await(ht_cond_t *, ht_mutex_t *, ht_event_t);
extern int            ht_cond_notify(ht_cond_t *, int);
extern int            ht_barrier_init(ht_barrier_t *, int);
extern int            ht_barrier_reach(ht_barrier_t *);

    /* user-space context functions */
extern int            ht_uctx_create(ht_uctx_t *);
extern int            ht_uctx_make(ht_uctx_t, char *, size_t, const sigset_t *, void (*)(void *), void *, ht_uctx_t);
extern int            ht_uctx_switch(ht_uctx_t, ht_uctx_t);
extern int            ht_uctx_destroy(ht_uctx_t);

    /* extension functions */
extern Sfdisc_t      *ht_sfiodisc(void);

    /* generalized variants of replacement functions */
extern int            ht_connect_ev(int, const struct sockaddr *, socklen_t, ht_event_t);
extern int            ht_accept_ev(int, struct sockaddr *, socklen_t *, ht_event_t);
extern int            ht_select_ev(int, fd_set *, fd_set *, fd_set *, struct timeval *, ht_event_t);
extern int            ht_poll_ev(struct pollfd *, nfds_t, int, ht_event_t);
extern ssize_t        ht_read_ev(int, void *, size_t, ht_event_t);
extern ssize_t        ht_write_ev(int, const void *, size_t, ht_event_t);
extern ssize_t        ht_readv_ev(int, const struct iovec *, int, ht_event_t);
extern ssize_t        ht_writev_ev(int, const struct iovec *, int, ht_event_t);
extern ssize_t        ht_recv_ev(int, void *, size_t, int, ht_event_t);
extern ssize_t        ht_send_ev(int, const void *, size_t, int, ht_event_t);
extern ssize_t        ht_recvfrom_ev(int, void *, size_t, int, struct sockaddr *, socklen_t *, ht_event_t);
extern ssize_t        ht_sendto_ev(int, const void *, size_t, int, const struct sockaddr *, socklen_t, ht_event_t);

    /* standard replacement functions */
extern int            ht_nanosleep(const struct timespec *, struct timespec *);
extern int            ht_usleep(unsigned int);
extern unsigned int   ht_sleep(unsigned int);
extern int            ht_connect(int, const struct sockaddr *, socklen_t);
extern int            ht_accept(int, struct sockaddr *, socklen_t *);
extern int            ht_select(int, fd_set *, fd_set *, fd_set *, struct timeval *);
extern int            ht_pselect(int, fd_set *, fd_set *, fd_set *, const struct timespec *, const sigset_t *);
extern int            ht_poll(struct pollfd *, nfds_t, int);
extern ssize_t        ht_read(int, void *, size_t);
extern ssize_t        ht_write(int, const void *, size_t);
extern ssize_t        ht_readv(int, const struct iovec *, int);
extern ssize_t        ht_writev(int, const struct iovec *, int);
extern ssize_t        ht_recv(int, void *, size_t, int);
extern ssize_t        ht_send(int, const void *, size_t, int);
extern ssize_t        ht_recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *);
extern ssize_t        ht_sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
extern ssize_t        ht_pread(int, void *, size_t, off_t);
extern ssize_t        ht_pwrite(int, const void *, size_t, off_t);

END_DECLARATION

    /* soft system call mapping support */
#if HT_SYSCALL_SOFT && !defined(_HT_PRIVATE)
#define fork          ht_fork
#define waitpid       ht_waitpid
#define system        ht_system
#define nanosleep     ht_nanosleep
#define usleep        ht_usleep
#define sleep         ht_sleep
#define sigprocmask   ht_sigmask
#define sigwait       ht_sigwait
#define select        ht_select
#define pselect       ht_pselect
#define poll          ht_poll
#define connect       ht_connect
#define accept        ht_accept
#define read          ht_read
#define write         ht_write
#define readv         ht_readv
#define writev        ht_writev
#define recv          ht_recv
#define send          ht_send
#define recvfrom      ht_recvfrom
#define sendto        ht_sendto
#define pread         ht_pread
#define pwrite        ht_pwrite
#endif

    /* backward compatibility (Pth < 1.5.0) */
#define ht_event_occurred(ev) \
    (   ht_event_status(ev) == HT_STATUS_OCCURRED \
     || ht_event_status(ev) == HT_STATUS_FAILED   )

#endif /* _HT_H_ */


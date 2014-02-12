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
#include <setjmp.h>
#include <signal.h>
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


#endif /* _HT_P_H_ */

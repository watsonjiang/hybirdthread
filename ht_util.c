#include "ht_p.h"

/* calculate numerical mimimum */
#define ht_util_min(a,b) \
        ((a) > (b) ? (b) : (a))

/* copy a string like strncpy() but always null-terminate */
char *
ht_util_cpystrn(char *dst, const char *src, size_t dst_size)
{
    register char *d, *end;

    if (dst_size == 0)
        return dst;
    d = dst;
    end = dst + dst_size - 1;
    for (; d < end; ++d, ++src) {
        if ((*d = *src) == '\0')
            return d;
    }
    *d = '\0';
    return d;
}

/* check whether a file-descriptor is valid */
int 
ht_util_fd_valid(int fd)
{
    if (fd < 0 || fd >= FD_SETSIZE)
        return FALSE;
    if (fcntl(fd, F_GETFL) == -1 && errno == EBADF)
        return FALSE;
    return TRUE;
}

/* merge input fd set into output fds */
void 
ht_util_fds_merge(int nfd,
                  fd_set *ifds1, fd_set *ofds1,
                  fd_set *ifds2, fd_set *ofds2,
                  fd_set *ifds3, fd_set *ofds3)
{
    register int s;

    for (s = 0; s < nfd; s++) {
        if (ifds1 != NULL)
            if (FD_ISSET(s, ifds1))
                FD_SET(s, ofds1);
        if (ifds2 != NULL)
            if (FD_ISSET(s, ifds2))
                FD_SET(s, ofds2);
        if (ifds3 != NULL)
            if (FD_ISSET(s, ifds3))
                FD_SET(s, ofds3);
    }
    return;
}

/* test whether fds in the input fd sets occurred in the output fds */
int 
ht_util_fds_test(int nfd,
                             fd_set *ifds1, fd_set *ofds1,
                             fd_set *ifds2, fd_set *ofds2,
                             fd_set *ifds3, fd_set *ofds3)
{
    register int s;

    for (s = 0; s < nfd; s++) {
        if (ifds1 != NULL)
            if (FD_ISSET(s, ifds1) && FD_ISSET(s, ofds1))
                return TRUE;
        if (ifds2 != NULL)
            if (FD_ISSET(s, ifds2) && FD_ISSET(s, ofds2))
                return TRUE;
        if (ifds3 != NULL)
            if (FD_ISSET(s, ifds3) && FD_ISSET(s, ofds3))
                return TRUE;
    }
    return FALSE;
}

/*
 * clear fds in input fd sets if not occurred in output fd sets and return
 * number of remaining input fds. This number uses BSD select(2) semantics: a
 * fd in two set counts twice!
 */
int 
ht_util_fds_select(int nfd,
                   fd_set *ifds1, fd_set *ofds1,
                   fd_set *ifds2, fd_set *ofds2,
                   fd_set *ifds3, fd_set *ofds3)
{
    register int s;
    register int n;

    n = 0;
    for (s = 0; s < nfd; s++) {
        if (ifds1 != NULL && FD_ISSET(s, ifds1)) {
            if (!FD_ISSET(s, ofds1))
               FD_CLR(s, ifds1);
            else
               n++;
        }
        if (ifds2 != NULL && FD_ISSET(s, ifds2)) {
            if (!FD_ISSET(s, ofds2))
                FD_CLR(s, ifds2);
            else
                n++;
        }
        if (ifds3 != NULL && FD_ISSET(s, ifds3)) {
            if (!FD_ISSET(s, ofds3))
                FD_CLR(s, ifds3);
            else
                n++;
        }
    }
    return n;
}


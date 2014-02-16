#include "ht_p.h"
/* a global variable holding a zero time */
ht_time_t ht_time_zero = { 0L, 0L };

/* sleep for a specified amount of microseconds */
void 
ht_time_usleep(unsigned long usec)
{
    usleep((unsigned int )usec);
    return;
}

/* time value constructor */
ht_time_t 
ht_time(long sec, long usec)
{
    ht_time_t tv;

    tv.tv_sec  = sec;
    tv.tv_usec = usec;
    return tv;
}

/* timeout value constructor */
ht_time_t 
ht_timeout(long sec, long usec)
{
    ht_time_t tv;
    ht_time_t tvd;

    ht_time_set(&tv, HT_TIME_NOW);
    tvd.tv_sec  = sec;
    tvd.tv_usec = usec;
    ht_time_add(&tv, &tvd);
    return tv;
}

/* calculate: t1 <=> t2 */
int 
ht_time_cmp(ht_time_t *t1, ht_time_t *t2)
{
    int rc;

    rc = t1->tv_sec - t2->tv_sec;
    if (rc == 0)
         rc = t1->tv_usec - t2->tv_usec;
    return rc;
}

/* calculate: t1 = t1 / n */
void 
ht_time_div(ht_time_t *t1, int n)
{
    long q, r;

    q = (t1->tv_sec / n);
    r = (((t1->tv_sec % n) * 1000000) / n) + (t1->tv_usec / n);
    if (r > 1000000) {
        q += 1;
        r -= 1000000;
    }
    t1->tv_sec  = q;
    t1->tv_usec = r;
    return;
}

/* calculate: t1 = t1 * n */
void 
ht_time_mul(ht_time_t *t1, int n)
{
    t1->tv_sec  *= n;
    t1->tv_usec *= n;
    t1->tv_sec  += (t1->tv_usec / 1000000);
    t1->tv_usec  = (t1->tv_usec % 1000000);
    return;
}

/* convert a time structure into a double value */
double 
ht_time_t2d(ht_time_t *t)
{
    double d;

    d = ((double)t->tv_sec*1000000 + (double)t->tv_usec) / 1000000;
    return d;
}

/* convert a time structure into a integer value */
int 
ht_time_t2i(ht_time_t *t)
{
    int i;

    i = (t->tv_sec*1000000 + t->tv_usec) / 1000000;
    return i;
}

/* check whether time is positive */
int 
ht_time_pos(ht_time_t *t)
{
    if (t->tv_sec > 0 && t->tv_usec > 0)
        return 1;
    else
        return 0;
}


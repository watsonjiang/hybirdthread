#include "ht.h"
#include "ht_p.h"

enum {
    HT_ATTR_GET,
    HT_ATTR_SET
};

struct ht_attr_st {
    ht_t        a_tid;
    int          a_prio;
    int          a_dispatches;
    char         a_name[HT_TCB_NAMELEN];
    int          a_joinable;
    unsigned int a_cancelstate;
    unsigned int a_stacksize;
    char        *a_stackaddr;
};

ht_attr_t ht_attr_of(ht_t t)
{
    ht_attr_t a;

    if (t == NULL)
        return ht_error((ht_attr_t)NULL, EINVAL);
    if ((a = (ht_attr_t)malloc(sizeof(struct ht_attr_st))) == NULL)
        return ht_error((ht_attr_t)NULL, ENOMEM);
    a->a_tid = t;
    return a;
}

ht_attr_t ht_attr_new(void)
{
    ht_attr_t a;

    if ((a = (ht_attr_t)malloc(sizeof(struct ht_attr_st))) == NULL)
        return ht_error((ht_attr_t)NULL, ENOMEM);
    a->a_tid = NULL;
    ht_attr_init(a);
    return a;
}

int ht_attr_destroy(ht_attr_t a)
{
    if (a == NULL)
        return ht_error(FALSE, EINVAL);
    free(a);
    return TRUE;
}

int ht_attr_init(ht_attr_t a)
{
    if (a == NULL)
        return ht_error(FALSE, EINVAL);
    if (a->a_tid != NULL)
        return ht_error(FALSE, EPERM);
    a->a_prio = HT_PRIO_STD;
    ht_util_cpystrn(a->a_name, "unknown", HT_TCB_NAMELEN);
    a->a_dispatches = 0;
    a->a_joinable = TRUE;
    a->a_cancelstate = HT_CANCEL_DEFAULT;
    a->a_stacksize = 64*1024;
    a->a_stackaddr = NULL;
    return TRUE;
}

int ht_attr_get(ht_attr_t a, int op, ...)
{
    va_list ap;
    int rc;

    va_start(ap, op);
    rc = ht_attr_ctrl(HT_ATTR_GET, a, op, ap);
    va_end(ap);
    return rc;
}

int ht_attr_set(ht_attr_t a, int op, ...)
{
    va_list ap;
    int rc;

    va_start(ap, op);
    rc = ht_attr_ctrl(HT_ATTR_SET, a, op, ap);
    va_end(ap);
    return rc;
}

int ht_attr_ctrl(int cmd, ht_attr_t a, int op, va_list ap)
{
    if (a == NULL)
        return ht_error(FALSE, EINVAL);
    switch (op) {
        case HT_ATTR_PRIO: {
            /* priority */
            int val, *src, *dst;
            if (cmd == HT_ATTR_SET) {
                src = &val; val = va_arg(ap, int);
                dst = (a->a_tid != NULL ? &a->a_tid->prio : &a->a_prio);
            }
            else {
                src = (a->a_tid != NULL ? &a->a_tid->prio : &a->a_prio);
                dst = va_arg(ap, int *);
            }
            *dst = *src;
            break;
        }
        case HT_ATTR_NAME: {
            /* name */
            if (cmd == HT_ATTR_SET) {
                char *src, *dst;
                src = va_arg(ap, char *);
                dst = (a->a_tid != NULL ? a->a_tid->name : a->a_name);
                ht_util_cpystrn(dst, src, HT_TCB_NAMELEN);
            }
            else {
                char *src, **dst;
                src = (a->a_tid != NULL ? a->a_tid->name : a->a_name);
                dst = va_arg(ap, char **);
                *dst = src;
            }
            break;
        }
        case HT_ATTR_DISPATCHES: {
            /* incremented on every context switch */
            int val, *src, *dst;
            if (cmd == HT_ATTR_SET) {
                src = &val; val = va_arg(ap, int);
                dst = (a->a_tid != NULL ? &a->a_tid->dispatches : &a->a_dispatches);
            }
            else {
                src = (a->a_tid != NULL ? &a->a_tid->dispatches : &a->a_dispatches);
                dst = va_arg(ap, int *);
            }
            *dst = *src;
            break;
        }
        case HT_ATTR_JOINABLE: {
            /* detachment type */
            int val, *src, *dst;
            if (cmd == HT_ATTR_SET) {
                src = &val; val = va_arg(ap, int);
                dst = (a->a_tid != NULL ? &a->a_tid->joinable : &a->a_joinable);
            }
            else {
                src = (a->a_tid != NULL ? &a->a_tid->joinable : &a->a_joinable);
                dst = va_arg(ap, int *);
            }
            *dst = *src;
            break;
        }
        case HT_ATTR_CANCEL_STATE: {
            /* cancellation state */
            unsigned int val, *src, *dst;
            if (cmd == HT_ATTR_SET) {
                src = &val; val = va_arg(ap, unsigned int);
                dst = (a->a_tid != NULL ? &a->a_tid->cancelstate : &a->a_cancelstate);
            }
            else {
                src = (a->a_tid != NULL ? &a->a_tid->cancelstate : &a->a_cancelstate);
                dst = va_arg(ap, unsigned int *);
            }
            *dst = *src;
            break;
        }
        case HT_ATTR_STACK_SIZE: {
            /* stack size */
            unsigned int val, *src, *dst;
            if (cmd == HT_ATTR_SET) {
                if (a->a_tid != NULL)
                    return ht_error(FALSE, EPERM);
                src = &val; val = va_arg(ap, unsigned int);
                dst = &a->a_stacksize;
            }
            else {
                src = (a->a_tid != NULL ? &a->a_tid->stacksize : &a->a_stacksize);
                dst = va_arg(ap, unsigned int *);
            }
            *dst = *src;
            break;
        }
        case HT_ATTR_STACK_ADDR: {
            /* stack address */
            char *val, **src, **dst;
            if (cmd == HT_ATTR_SET) {
                if (a->a_tid != NULL)
                    return ht_error(FALSE, EPERM);
                src = &val; val = va_arg(ap, char *);
                dst = &a->a_stackaddr;
            }
            else {
                src = (a->a_tid != NULL ? &a->a_tid->stack : &a->a_stackaddr);
                dst = va_arg(ap, char **);
            }
            *dst = *src;
            break;
        }
        case HT_ATTR_TIME_SPAWN: {
            ht_time_t *dst;
            if (cmd == HT_ATTR_SET)
                return ht_error(FALSE, EPERM);
            dst = va_arg(ap, ht_time_t *);
            if (a->a_tid != NULL)
                ht_time_set(dst, &a->a_tid->spawned);
            else
                ht_time_set(dst, HT_TIME_ZERO);
            break;
        }
        case HT_ATTR_TIME_LAST: {
            ht_time_t *dst;
            if (cmd == HT_ATTR_SET)
                return ht_error(FALSE, EPERM);
            dst = va_arg(ap, ht_time_t *);
            if (a->a_tid != NULL)
                ht_time_set(dst, &a->a_tid->lastran);
            else
                ht_time_set(dst, HT_TIME_ZERO);
            break;
        }
        case HT_ATTR_TIME_RAN: {
            ht_time_t *dst;
            if (cmd == HT_ATTR_SET)
                return ht_error(FALSE, EPERM);
            dst = va_arg(ap, ht_time_t *);
            if (a->a_tid != NULL)
                ht_time_set(dst, &a->a_tid->running);
            else
                ht_time_set(dst, HT_TIME_ZERO);
            break;
        }
        case HT_ATTR_START_FUNC: {
            void *(**dst)(void *);
            if (cmd == HT_ATTR_SET)
                return ht_error(FALSE, EPERM);
            if (a->a_tid == NULL)
                return ht_error(FALSE, EACCES);
            dst = (void *(**)(void *))va_arg(ap, void *);
            *dst = a->a_tid->start_func;
            break;
        }
        case HT_ATTR_START_ARG: {
            void **dst;
            if (cmd == HT_ATTR_SET)
                return ht_error(FALSE, EPERM);
            if (a->a_tid == NULL)
                return ht_error(FALSE, EACCES);
            dst = va_arg(ap, void **);
            *dst = a->a_tid->start_arg;
            break;
        }
        case HT_ATTR_STATE: {
            ht_state_t *dst;
            if (cmd == HT_ATTR_SET)
                return ht_error(FALSE, EPERM);
            if (a->a_tid == NULL)
                return ht_error(FALSE, EACCES);
            dst = va_arg(ap, ht_state_t *);
            *dst = a->a_tid->state;
            break;
        }
        case HT_ATTR_EVENTS: {
            ht_event_t *dst;
            if (cmd == HT_ATTR_SET)
                return ht_error(FALSE, EPERM);
            if (a->a_tid == NULL)
                return ht_error(FALSE, EACCES);
            dst = va_arg(ap, ht_event_t *);
            *dst = a->a_tid->events;
            break;
        }
        case HT_ATTR_BOUND: {
            int *dst;
            if (cmd == HT_ATTR_SET)
                return ht_error(FALSE, EPERM);
            dst = va_arg(ap, int *);
            *dst = (a->a_tid != NULL ? TRUE : FALSE);
            break;
        }
        default:
            return ht_error(FALSE, EINVAL);
    }
    return TRUE;
}


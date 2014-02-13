#include "ht_p.h"

const char *ht_state_names[] = {
    "scheduler", "new", "ready", "running", "waiting", "dead"
};

#define SIGSTKSZ 8192

/* allocate a thread control block */
ht_t ht_tcb_alloc(unsigned int stacksize, void *stackaddr)
{
    ht_t t;

    if (stacksize > 0 && stacksize < SIGSTKSZ)
        stacksize = SIGSTKSZ;
    if ((t = (ht_t)malloc(sizeof(struct ht_st))) == NULL)
        return NULL;
    t->stacksize  = stacksize;
    t->stack      = NULL;
    t->stackguard = NULL;
    t->stackloan  = (stackaddr != NULL ? TRUE : FALSE);
    if (stacksize > 0) { /* stacksize == 0 means "main" thread */
        if (stackaddr != NULL)
            t->stack = (char *)(stackaddr);
        else {
            if ((t->stack = (char *)malloc(stacksize)) == NULL) {
                ht_shield { free(t); }
                return NULL;
            }
        }
        /* guard is at lowest address (alignment is guarrantied) */
        t->stackguard = (long *)((long)t->stack); /* double cast to avoid alignment warning */
        *t->stackguard = 0xDEAD;
    }
    return t;
}

/* free a thread control block */
void ht_tcb_free(ht_t t)
{
    if (t == NULL)
        return;
    if (t->stack != NULL && !t->stackloan)
        free(t->stack);
    if (t->data_value != NULL)
        free(t->data_value);
    if (t->cleanups != NULL)
        ht_cleanup_popall(t, FALSE);
    free(t);
    return;
}


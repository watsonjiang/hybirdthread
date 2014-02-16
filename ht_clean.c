#include "ht_p.h"

int 
ht_cleanup_push(void (*func)(void *), void *arg)
{
    ht_cleanup_t *cleanup;

    if (func == NULL)
        return ht_error(FALSE, EINVAL);
    if ((cleanup = (ht_cleanup_t *)malloc(sizeof(ht_cleanup_t))) == NULL)
        return ht_error(FALSE, ENOMEM);
    cleanup->func = func;
    cleanup->arg  = arg;
    cleanup->next = ht_current->cleanups;
    ht_current->cleanups = cleanup;
    return TRUE;
}

int 
ht_cleanup_pop(int execute)
{
    ht_cleanup_t *cleanup;
    int rc;

    rc = FALSE;
    if ((cleanup = ht_current->cleanups) != NULL) {
        ht_current->cleanups = cleanup->next;
        if (execute)
            cleanup->func(cleanup->arg);
        free(cleanup);
        rc = TRUE;
    }
    return rc;
}

void 
ht_cleanup_popall(ht_t t, int execute)
{
    ht_cleanup_t *cleanup;

    while ((cleanup = t->cleanups) != NULL) {
        t->cleanups = cleanup->next;
        if (execute)
            cleanup->func(cleanup->arg);
        free(cleanup);
    }
    return;
}


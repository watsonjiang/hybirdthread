#include "ht_p.h"

/* user-space context structure */
struct ht_uctx_st {
    int         uc_stack_own; /* whether stack were allocated by us */
    char       *uc_stack_ptr; /* pointer to start address of stack area */
    size_t      uc_stack_len; /* size of stack area */
    int         uc_mctx_set;  /* whether uc_mctx is set */
    ht_mctx_t  uc_mctx;      /* saved underlying machine context */
};

/* create user-space context structure */
int
ht_uctx_create(
    ht_uctx_t *puctx)
{
    ht_uctx_t uctx;

    /* argument sanity checking */
    if (puctx == NULL)
        return ht_error(FALSE, EINVAL);

    /* allocate the context structure */
    if ((uctx = (ht_uctx_t)malloc(sizeof(struct ht_uctx_st))) == NULL)
        return ht_error(FALSE, errno);

    /* initialize the context structure */
    uctx->uc_stack_own = FALSE;
    uctx->uc_stack_ptr = NULL;
    uctx->uc_stack_len = 0;
    uctx->uc_mctx_set  = FALSE;
    memset((void *)&uctx->uc_mctx, 0, sizeof(ht_mctx_t));

    /* pass result to caller */
    *puctx = uctx;

    return TRUE;
}

/* trampoline context */
typedef struct {
    ht_mctx_t *mctx_parent;
    ht_uctx_t  uctx_this;
    ht_uctx_t  uctx_after;
    void      (*start_func)(void *);
    void       *start_arg;
} ht_uctx_trampoline_t;
ht_uctx_trampoline_t ht_uctx_trampoline_ctx;

/* trampoline function for ht_uctx_make() */
static void ht_uctx_trampoline(void)
{
    volatile ht_uctx_trampoline_t ctx;

    /* move context information from global to local storage */
    ctx.mctx_parent = ht_uctx_trampoline_ctx.mctx_parent;
    ctx.uctx_this   = ht_uctx_trampoline_ctx.uctx_this;
    ctx.uctx_after  = ht_uctx_trampoline_ctx.uctx_after;
    ctx.start_func  = ht_uctx_trampoline_ctx.start_func;
    ctx.start_arg   = ht_uctx_trampoline_ctx.start_arg;

    /* switch back to parent */
    ht_mctx_switch(&(ctx.uctx_this->uc_mctx), ctx.mctx_parent);

    /* enter start function */
    (*ctx.start_func)(ctx.start_arg);

    /* switch to successor user-space context */
    if (ctx.uctx_after != NULL)
        ht_mctx_restore(&(ctx.uctx_after->uc_mctx));

    /* terminate process (the only reasonable thing to do here) */
    exit(0);

    /* NOTREACHED */
    return;
}

/* make setup of user-space context structure */
int
ht_uctx_make(
    ht_uctx_t uctx,
    char *sk_addr, size_t sk_size,
    const sigset_t *sigmask,
    void (*start_func)(void *), void *start_arg,
    ht_uctx_t uctx_after)
{
    ht_mctx_t mctx_parent;
    sigset_t ss;

    /* argument sanity checking */
    if (uctx == NULL || start_func == NULL || sk_size < 16*1024)
        return ht_error(FALSE, EINVAL);

    /* configure run-time stack */
    if (sk_addr == NULL) {
        if ((sk_addr = (char *)malloc(sk_size)) == NULL)
            return ht_error(FALSE, errno);
        uctx->uc_stack_own = TRUE;
    }
    else
        uctx->uc_stack_own = FALSE;
    uctx->uc_stack_ptr = sk_addr;
    uctx->uc_stack_len = sk_size;

    /* configure the underlying machine context */
    if (!ht_mctx_set(&uctx->uc_mctx, ht_uctx_trampoline,
                      uctx->uc_stack_ptr, uctx->uc_stack_ptr+uctx->uc_stack_len))
        return ht_error(FALSE, errno);

    /* move context information into global storage for the trampoline jump */
    ht_uctx_trampoline_ctx.mctx_parent = &mctx_parent;
    ht_uctx_trampoline_ctx.uctx_this   = uctx;
    ht_uctx_trampoline_ctx.uctx_after  = uctx_after;
    ht_uctx_trampoline_ctx.start_func  = start_func;
    ht_uctx_trampoline_ctx.start_arg   = start_arg;

    /* optionally establish temporary signal mask */
    if (sigmask != NULL)
        sigprocmask(SIG_SETMASK, sigmask, &ss);

    /* perform the trampoline step */
    ht_mctx_switch(&mctx_parent, &(uctx->uc_mctx));

    /* optionally restore original signal mask */
    if (sigmask != NULL)
        sigprocmask(SIG_SETMASK, &ss, NULL);

    /* finally flag that the context is now configured */
    uctx->uc_mctx_set = TRUE;

    return TRUE;
}

/* switch from current to other user-space context */
int
ht_uctx_switch(
    ht_uctx_t uctx_from,
    ht_uctx_t uctx_to)
{
    /* argument sanity checking */
    if (uctx_from == NULL || uctx_to == NULL)
        return ht_error(FALSE, EINVAL);
    if (!(uctx_to->uc_mctx_set))
        return ht_error(FALSE, EPERM);

    /* switch underlying machine context */
    uctx_from->uc_mctx_set = TRUE;
    ht_mctx_switch(&(uctx_from->uc_mctx), &(uctx_to->uc_mctx));

    return TRUE;
}

/* destroy user-space context structure */
int
ht_uctx_destroy(
    ht_uctx_t uctx)
{
    /* argument sanity checking */
    if (uctx == NULL)
        return ht_error(FALSE, EINVAL);

    /* deallocate dynamically allocated stack */
    if (uctx->uc_stack_own && uctx->uc_stack_ptr != NULL)
        free(uctx->uc_stack_ptr);

    /* deallocate context structure */
    free(uctx);

    return TRUE;
}


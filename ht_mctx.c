#include "ht_p.h"


/*
** ____ MACHINE STATE INITIALIZATION ________________________________
*/

/*
 * VARIANT 1: THE STANDARDIZED SVR4/SUSv2 APPROACH
 *
 * This is the preferred variant, because it uses the standardized
 * SVR4/SUSv2 makecontext(2) and friends which is a facility intended
 * for user-space context switching. The thread creation therefore is
 * straight-foreward.
 */

int 
ht_mctx_set(
    ht_mctx_t *mctx, void (*func)(void), char *sk_addr_lo, char *sk_addr_hi)
{
    /* fetch current context */
    if (getcontext(&(mctx->uc)) != 0)
        return FALSE;

    /* remove parent link */
    mctx->uc.uc_link           = NULL;

    /* configure new stack */
    mctx->uc.uc_stack.ss_sp    = sk_addr_lo;
    mctx->uc.uc_stack.ss_size  = sk_addr_hi-sk_addr_lo;
    mctx->uc.uc_stack.ss_flags = 0;

    /* configure startup function (with no arguments) */
    makecontext(&(mctx->uc), func, 0+1);

    return TRUE;
}


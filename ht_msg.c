#include "ht_p.h"

#pragma GCC diagnostic ignored "-Waddress"

static ht_ring_t ht_msgport = HT_RING_INIT;

/* create a new message port */
ht_msgport_t ht_msgport_create(const char *name)
{
    ht_msgport_t mp;

    /* Notice: "name" is allowed to be NULL */

    /* allocate message port structure */
    if ((mp = (ht_msgport_t)malloc(sizeof(struct ht_msgport_st))) == NULL)
        return ht_error((ht_msgport_t)NULL, ENOMEM);

    /* initialize structure */
    mp->mp_name  = name;
    mp->mp_tid   = ht_current;
    ht_ring_init(&mp->mp_queue);

    /* insert into list of existing message ports */
    ht_ring_append(&ht_msgport, &mp->mp_node);

    return mp;
}

/* delete a message port */
void ht_msgport_destroy(ht_msgport_t mp)
{
    ht_message_t *m;

    /* check input */
    if (mp == NULL)
        return;

    /* first reply to all pending messages */
    while ((m = ht_msgport_get(mp)) != NULL)
        ht_msgport_reply(m);

    /* remove from list of existing message ports */
    ht_ring_delete(&ht_msgport, &mp->mp_node);

    /* deallocate message port structure */
    free(mp);

    return;
}

/* find a known message port through name */
ht_msgport_t ht_msgport_find(const char *name)
{
    ht_msgport_t mp, mpf;

    /* check input */
    if (name == NULL)
        return ht_error((ht_msgport_t)NULL, EINVAL);

    /* iterate over message ports */
    mp = mpf = (ht_msgport_t)ht_ring_first(&ht_msgport);
    while (mp != NULL) {
        if (mp->mp_name != NULL)
            if (strcmp(mp->mp_name, name) == 0)
                break;
        mp = (ht_msgport_t)ht_ring_next(&ht_msgport, (ht_ringnode_t *)mp);
        if (mp == mpf) {
            mp = NULL;
            break;
        }
    }
    return mp;
}

/* number of messages on a port */
int ht_msgport_pending(ht_msgport_t mp)
{
    if (mp == NULL)
        return ht_error(-1, EINVAL);
    return ht_ring_elements(&mp->mp_queue);
}

/* put a message on a port */
int ht_msgport_put(ht_msgport_t mp, ht_message_t *m)
{
    if (mp == NULL)
        return ht_error(FALSE, EINVAL);
    ht_ring_append(&mp->mp_queue, (ht_ringnode_t *)m);
    return TRUE;
}

/* get top message from a port */
ht_message_t *ht_msgport_get(ht_msgport_t mp)
{
    ht_message_t *m;

    if (mp == NULL)
        return ht_error((ht_message_t *)NULL, EINVAL);
    m = (ht_message_t *)ht_ring_pop(&mp->mp_queue);
    return m;
}

/* reply message to sender */
int ht_msgport_reply(ht_message_t *m)
{
    if (m == NULL)
        return ht_error(FALSE, EINVAL);
    return ht_msgport_put(m->m_replyport, m);
}


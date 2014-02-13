/*
** priority queue implementation
*/
#include "ht_p.h"

/* initialize a priority queue; O(1) */
void ht_pqueue_init(ht_pqueue_t *q)
{
    if (q != NULL) {
        q->q_head = NULL;
        q->q_num  = 0;
    }
    return;
}

/* insert thread into priority queue; O(n) */
void ht_pqueue_insert(ht_pqueue_t *q, int prio, ht_t t)
{
    ht_t c;
    int p;

    if (q == NULL)
        return;
    if (q->q_head == NULL || q->q_num == 0) {
        /* add as first element */
        t->q_prev = t;
        t->q_next = t;
        t->q_prio = prio;
        q->q_head = t;
    }
    else if (q->q_head->q_prio < prio) {
        /* add as new head of queue */
        t->q_prev = q->q_head->q_prev;
        t->q_next = q->q_head;
        t->q_prev->q_next = t;
        t->q_next->q_prev = t;
        t->q_prio = prio;
        t->q_next->q_prio = prio - t->q_next->q_prio;
        q->q_head = t;
    }
    else {
        /* insert after elements with greater or equal priority */
        c = q->q_head;
        p = c->q_prio;
        while ((p - c->q_next->q_prio) >= prio && c->q_next != q->q_head) {
            c = c->q_next;
            p -= c->q_prio;
        }
        t->q_prev = c;
        t->q_next = c->q_next;
        t->q_prev->q_next = t;
        t->q_next->q_prev = t;
        t->q_prio = p - prio;
        if (t->q_next != q->q_head)
            t->q_next->q_prio -= t->q_prio;
    }
    q->q_num++;
    return;
}

/* remove thread with maximum priority from priority queue; O(1) */
ht_t ht_pqueue_delmax(ht_pqueue_t *q)
{
    ht_t t;

    if (q == NULL)
        return NULL;
    if (q->q_head == NULL)
        t = NULL;
    else if (q->q_head->q_next == q->q_head) {
        /* remove the last element and make queue empty */
        t = q->q_head;
        t->q_next = NULL;
        t->q_prev = NULL;
        t->q_prio = 0;
        q->q_head = NULL;
        q->q_num  = 0;
    }
    else {
        /* remove head of queue */
        t = q->q_head;
        t->q_prev->q_next = t->q_next;
        t->q_next->q_prev = t->q_prev;
        t->q_next->q_prio = t->q_prio - t->q_next->q_prio;
        t->q_prio = 0;
        q->q_head = t->q_next;
        q->q_num--;
    }
    return t;
}

/* remove thread from priority queue; O(n) */
void ht_pqueue_delete(ht_pqueue_t *q, ht_t t)
{
    if (q == NULL)
        return;
    if (q->q_head == NULL)
        return;
    else if (q->q_head == t) {
        if (t->q_next == t) {
            /* remove the last element and make queue empty */
            t->q_next = NULL;
            t->q_prev = NULL;
            t->q_prio = 0;
            q->q_head = NULL;
            q->q_num  = 0;
        }
        else {
            /* remove head of queue */
            t->q_prev->q_next = t->q_next;
            t->q_next->q_prev = t->q_prev;
            t->q_next->q_prio = t->q_prio - t->q_next->q_prio;
            t->q_prio = 0;
            q->q_head = t->q_next;
            q->q_num--;
        }
    }
    else {
        t->q_prev->q_next = t->q_next;
        t->q_next->q_prev = t->q_prev;
        if (t->q_next != q->q_head)
            t->q_next->q_prio += t->q_prio;
        t->q_prio = 0;
        q->q_num--;
    }
    return;
}

/* determine priority required to favorite a thread; O(1) */
#if cpp
#define ht_pqueue_favorite_prio(q) \
    ((q)->q_head != NULL ? (q)->q_head->q_prio + 1 : HT_PRIO_MAX)
#endif

/* move a thread inside queue to the top; O(n) */
int ht_pqueue_favorite(ht_pqueue_t *q, ht_t t)
{
    if (q == NULL)
        return FALSE;
    if (q->q_head == NULL || q->q_num == 0)
        return FALSE;
    /* element is already at top */
    if (q->q_num == 1)
        return TRUE;
    /* move to top */
    ht_pqueue_delete(q, t);
    ht_pqueue_insert(q, ht_pqueue_favorite_prio(q), t);
    return TRUE;
}

/* increase priority of all(!) threads in queue; O(1) */
void ht_pqueue_increase(ht_pqueue_t *q)
{
    if (q == NULL)
        return;
    if (q->q_head == NULL)
        return;
    /* <grin> yes, that's all ;-) */
    q->q_head->q_prio += 1;
    return;
}

/* walk to last thread in queue */
ht_t ht_pqueue_tail(ht_pqueue_t *q)
{
    if (q == NULL)
        return NULL;
    if (q->q_head == NULL)
        return NULL;
    return q->q_head->q_prev;
}

/* walk to next or previous thread in queue; O(1) */
ht_t ht_pqueue_walk(ht_pqueue_t *q, ht_t t, int direction)
{
    ht_t tn;

    if (q == NULL || t == NULL)
        return NULL;
    tn = NULL;
    if (direction == HT_WALK_PREV) {
        if (t != q->q_head)
            tn = t->q_prev;
    }
    else if (direction == HT_WALK_NEXT) {
        tn = t->q_next;
        if (tn == q->q_head)
            tn = NULL;
    }
    return tn;
}

/* check whether a thread is in a queue; O(n) */
int ht_pqueue_contains(ht_pqueue_t *q, ht_t t)
{
    ht_t tc;
    int found;

    found = FALSE;
    for (tc = ht_pqueue_head(q); tc != NULL;
         tc = ht_pqueue_walk(q, tc, HT_WALK_NEXT)) {
        if (tc == t) {
            found = TRUE;
            break;
        }
    }
    return found;
}


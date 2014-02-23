#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "ht_p.h"

#include "ht_test.h"

/* our simple query structure */
struct query {
    ht_message_t head; /* the standard message header */
    char *string;       /* the query ingredients */
};

/* our worker thread which translates the string to upper case */
typedef struct {
    ht_msgport_t mp;
    ht_event_t ev;
} worker_cleanup_t;
static void worker_cleanup(void *arg)
{
    worker_cleanup_t *wc = (worker_cleanup_t *)arg;
    ht_event_free(wc->ev, HT_FREE_THIS);
    ht_msgport_destroy(wc->mp);
    return;
}
static void *worker(void *_dummy)
{
    worker_cleanup_t wc;
    ht_msgport_t mp;
    ht_event_t ev;
    struct query *q;
    int i;

    ht_debug1("worker: start");
    wc.mp = mp = ht_msgport_create("worker");
    wc.ev = ev = ht_event(HT_EVENT_MSG, mp);
    ht_cleanup_push(worker_cleanup, &wc);
    for (;;) {
         if ((i = ht_wait(ev)) != 1)
             continue;
         while ((q = (struct query *)ht_msgport_get(mp)) != NULL) {
             ht_debug2("worker: recv query <%s>", q->string);
             for (i = 0; q->string[i] != NUL; i++)
                 q->string[i] = toupper(q->string[i]);
             ht_debug2("worker: send reply <%s>", q->string);
             ht_msgport_reply((ht_message_t *)q);
         }
    }
    return NULL;
}


#define MAXLINELEN 1024

int main(int argc, char *argv[])
{
    char caLine[MAXLINELEN];
    ht_event_t ev = NULL;
    ht_event_t evt = NULL;
    ht_t t_worker = NULL;
    ht_attr_t t_attr;
    ht_msgport_t mp = NULL;
    ht_msgport_t mp_worker = NULL;
    struct query *q = NULL;

    if (!ht_init()) {
        perror("ht_init");
        exit(1);
    }

    t_attr = ht_attr_new();
    ht_attr_set(t_attr, HT_ATTR_NAME, "worker");
    ht_attr_set(t_attr, HT_ATTR_JOINABLE, TRUE);
    ht_attr_set(t_attr, HT_ATTR_STACK_SIZE, 16*1024);
    t_worker = ht_spawn(t_attr, worker, NULL);
    ht_attr_destroy(t_attr);
    ht_yield(NULL);

    mp_worker = ht_msgport_find("worker");
    mp = ht_msgport_create("main");
    q = (struct query *)malloc(sizeof(struct query));
    ev = ht_event(HT_EVENT_MSG, mp);

    evt = NULL;
    strcpy(caLine, "hello");
    q->string = caLine;
    q->head.m_replyport = mp;
    ht_msgport_put(mp_worker, (ht_message_t *)q);
    ht_wait(ev);
    q = (struct query *)ht_msgport_get(mp);
    HT_TEST_ASSERT(q != NULL, "ht_msgport_get returned NULL.");
    HT_TEST_ASSERT(0 == strcmp(q->string, "HELLO"), 
                    "message port did not work as expected.");

    free(q);
    ht_event_free(ev, HT_FREE_THIS);
    ht_event_free(evt, HT_FREE_THIS);
    ht_msgport_destroy(mp);
    ht_cancel(t_worker);
    ht_join(t_worker, NULL);
    ht_kill();
    return 0;
}


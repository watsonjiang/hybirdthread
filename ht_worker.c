/*
 * ht_hand_out, ht_get_back implementation
 */
#include "ht_p.h"


typedef struct ht_worker_ctx_st ht_worker_ctx_t;
struct ht_worker_ctx_st {
   ucontext_t worker_mctx;
   ht_t       task;
};

static pthread_key_t _ht_worker_ctx_key;
static pthread_mutex_t _ht_worker_start_mutex;   //used to sync the start of worker.
static pthread_cond_t _ht_worker_cond_started;
ht_tqueue_t  ht_TQ;         /* queue of tasks request to be exec by worker */

static 
void*
_ht_worker(void * argv)
{
   char buf[255] = {0};
   int id = *((int*) argv);
   ht_debug2("ht_worker: worker %d started.", id);
   pthread_mutex_lock(&_ht_worker_start_mutex);
   pthread_cond_signal(&_ht_worker_cond_started);
   pthread_mutex_unlock(&_ht_worker_start_mutex);
   ht_worker_ctx_t worker_ctx;
   pthread_setspecific(_ht_worker_ctx_key, &worker_ctx);
   while(1)
   {
      ht_t t = ht_tqueue_dequeue(&ht_TQ);
      snprintf(buf, 255, "worker %d switching to thread \"%s\"", 
                id, t->name);
      ht_debug2("ht_worker: %s", buf); 
      worker_ctx.task = t;
      swapcontext(&worker_ctx.worker_mctx, &t->mctx.uc);
      printf("4444444444\n");
      snprintf(buf, 255, "worker %d back from thread \"%s\"",
                id, t->name);
      ht_debug2("ht_worker: %s", buf);
      t->events->ev_args.TASK.fini = 1;   /* mark the fini flag,
                                             so schedule can notice the event
                                             and resched the thread. */
      
   }
   return 0;
}

int
ht_worker_init(int num_worker)
{
   /* initialize the mutex and cond */
   pthread_mutex_init(&_ht_worker_start_mutex, NULL);
   pthread_cond_init(&_ht_worker_cond_started, NULL);
   /* initialize the task queue */
   ht_tqueue_init(&ht_TQ, num_worker * 3); //allow 3 waiting tasks for each 
                                           //worker.
   /* start worker */
   pthread_key_create(&_ht_worker_ctx_key, NULL);
   pthread_t t;
   int i = 0;
   for(i = 0; i < num_worker; i++)
   {
      ht_debug2("ht_worker_init: starting worker %d", i);
      pthread_mutex_lock(&_ht_worker_start_mutex);
      pthread_create(&t, NULL, _ht_worker, (void*) &i);
      pthread_cond_wait(&_ht_worker_cond_started, &_ht_worker_start_mutex);
      pthread_mutex_unlock(&_ht_worker_start_mutex);
   }
   return 0;
}

int 
ht_hand_out()
{
	/* make a waiting ring 
		and link event ring to current thread */
   ht_event_t ev = ht_event(HT_EVENT_TASK); 
   ht_current->events = ev;
   /* set the thread to WAIT_FOR_SCHED_TO_WORKER 
	  and transfer control to scheduler */
	ht_current->state = HT_STATE_WAITING_FOR_SCHED_TO_WORKER;
   printf("1111111111111\n");
	ht_yield(NULL);
   printf("2222222222222i\n");
   return 0;
}

int 
ht_get_back()
{
   ht_worker_ctx_t* worker_ctx = (ht_worker_ctx_t*) pthread_getspecific(_ht_worker_ctx_key);
   ht_t t = worker_ctx->task;
   printf("333333333333\n");
   swapcontext(&t->mctx.uc, &worker_ctx->worker_mctx);
   return 0;
}

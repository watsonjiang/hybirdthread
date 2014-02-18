/*
 * ht_hand_out, ht_get_back implementation
 */
#include "ht_p.h"
typedef struct ht_task_st ht_task_t;
struct ht_task_st {
   ht_t           t_caller_ctx;
   int            t_fin_flag;
}
typedef struct ht_tqueue_st ht_tqueue_t;
struct ht_tqueue_st {
   ht_task_t*      q_buf;
   int             q_size;
   int             q_head;
   int             q_rear;
   pthread_mutex_t q_lock;
   pthread_cond_t  q_not_empty;
   pthread_cond_t  q_not_full;
}

void 
ht_tqueue_init(ht_tqueue_t * q, int size)
{
   q->q_buf = (ht_task_t*) malloc(sizeof(ht_task_t*) * size);
   q->q_size = size;
   q->q_not_full = PTHREAD_COND_INIT;
   q->q_not_empty = PTHREAD_COND_INIT;
   q->q_lock = PTHREAD_MUTEX_INIT;
   q->q_head = q->q_read = 0;
}

int 
ht_tqueue_elements(ht_tqueue_t * q)
{
   int n = q->q_head - q->q_read;
   if (n < 0)
      return -n;
   return n;
}

int
ht_tqueue_put(ht_tqueue_t * q, ht_task_t* task)
{
   pthread_mutex_lock(&q->q_lock);
   if(ht_tqueue_elements(q) == q->q_size)
      pthread_cond_wait(&q->q_not_full, &q->q_lock);
   q->q_head = (q->q_head + 1) % q->q_size;
   q->q_buf[q->q_head] = task;
   pthread_cond_signal(&q->q_not_empty);
   pthread_mutex_unlock(&q->q_lock);
   return 0;
}

ht_task_t*
ht_tqueue_get(ht_tqueue_t * q)
{
   pthread_mutex_lock(&q->q_lock);
   if(ht_tqueue_elements(q) == 0)
      pthread_cond_wait(&q->q_not_empty, &q->q_lock);
   ht_tqueue_t* r = q->q_buf[q->q_read];
   q->q_rear = (q->q_rear + 1) % q->q_size;
   pthread_cond_signal(&q->q_not_full);
   pthread_mutex_unlock(&q->q_lock);
   return r;
}

static
int
_ht_task_status_check_func(void * argv)
{
   ht_task_t * task = (ht_task_t*) argv;
   return task->t_fin_flag;
}

static pthread_key_t _ht_worker_ctx_key;
static pthread_mutex_t _ht_worker_start_mutex = PTHREAD_MUTEX_INITIALIZER;   //used to sync the start of worker.
static pthread_cond_t _ht_worker_cond_started = PTHREAD_COND_INITIALIZER;
static ht_tqueue_t ht_task_queue;


static 
void*
_ht_worker(void * not_used)
{
   pthread_mutex_lock(&_ht_worker_start_mutex);
   pthread_cond_signal(&_ht_worker_cond_started);
   pthread_mutex_unlock(&_ht_worker_start_mutex);

   ucontext_t worker_ctx;
   pthread_setspecific(_ht_worker_ctx_key, &worker_ctx);
   while(1)
   {
      ht_task_t * task = ht_tqueue_get(&ht_task_queue);
      swapcontext(&worker_ctx, &task->t_caller_ctx->mctx.uc);
   }
}

int
ht_worker_init(int num_worker)
{
   ht_tqueue_init(&ht_task_queue, num_worker * 3);
   pthread_key_create(&_ht_worker_ctx_key, NULL);
   pthread_t t;
   for(int i = 0; i < num_worker; i++)
   {
      pthread_mutex_lock(&_ht_worker_start_mutex);
      pthread_creaete(&t, NULL, _ht_worker, NULL);
      pthread_cond_wait(&_ht_worker_cond_started, &_ht_worker_start_mutex);
      pthread_mutex_unlock(&_ht_worker_start_mutex);
   }

}

int 
ht_hand_out()
{
   ht_task_t* task = malloc(sizeof(ht_task_t));
   task->t_caller_ctx = ht_self();
   task->t_fin_flag = 0;
   
}

int 
ht_get_back()
{

}

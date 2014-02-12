#include "ht.h"
#include <ucontext.h>
#include <pth.h>
#include "pth_p.h"
#include <pthread.h>
#include <memory.h>
#include "queue.h"

pthread_key_t _ht_worker_ctx_key;

struct _ht_task_t
{
   ucontext_t        task_start_point; //not use the caller's ctx due to sync 
                                       //problem.
   void *            task_exec_stack;
   pth_t             caller;
   bool              finished;
   pthread_mutex_t   lock;
   int               ref_count;
};

Queue<_ht_task_t*> _ht_taskqueue(128);

struct _ht_worker_ctx_t
{
   ucontext_t          worker_resume_ctx;
   _ht_task_t*          curr_task;
};

_ht_task_t * _ht_create_task()
{
   _ht_task_t * r = (_ht_task_t*) malloc(sizeof(_ht_task_t));
   r->task_exec_stack = NULL;
   pthread_mutex_init(&r->lock, NULL);
   r->ref_count = 0;
   r->finished = false;
   return r;
}

void _ht_add_task_ref(_ht_task_t* task)
{
   pthread_mutex_lock(&task->lock);
   task->ref_count ++;
   pthread_mutex_unlock(&task->lock);
}

void _ht_rm_task_ref(_ht_task_t* task)
{
   bool delete_flag = false;
   pthread_mutex_lock(&task->lock);
   task->ref_count --;
   if(task->ref_count == 0) 
   {
      delete_flag = true;
   }
   pthread_mutex_unlock(&task->lock);
   if(delete_flag)
   {
      free(task);
   }
}

static int _ht_task_status_check_func(void * argv)
{
   _ht_task_t* task = (_ht_task_t*) argv;
   return task->finished;     
}

pthread_mutex_t worker_start_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t worker_cond_started = PTHREAD_COND_INITIALIZER;

static void* _ht_async_worker(void * not_used)
{
   pthread_mutex_lock(&worker_start_mutex);
   pthread_cond_signal(&worker_cond_started);
   pthread_mutex_unlock(&worker_start_mutex);

   _ht_worker_ctx_t* worker_ctx = new _ht_worker_ctx_t();
   pthread_setspecific(_ht_worker_ctx_key, worker_ctx);
   char *task_exec_stack = (char*) malloc(256 * 1024);//set to 256k for each thread.
   for(;;)
   {
      _ht_task_t * task = _ht_taskqueue.pop();
      _ht_add_task_ref(task);
      worker_ctx->curr_task = task;
      task->task_exec_stack = task_exec_stack;
      memcpy(task_exec_stack, task->caller->stack, task->caller->stacksize);
      task->task_start_point.uc_mcontext.gregs[REG_EBP] = 
              (greg_t)task->task_exec_stack 
              + task->task_start_point.uc_mcontext.gregs[REG_EBP] 
              - (greg_t)task->caller->stack;
      task->task_start_point.uc_mcontext.gregs[REG_ESP] = 
              (greg_t)task->task_exec_stack
              + task->task_start_point.uc_mcontext.gregs[REG_ESP] 
              - (greg_t)task->caller->stack;
      
      swapcontext(&worker_ctx->worker_resume_ctx, &task->task_start_point);
   }
}

int ht_bg_exec_begin()
{
   pthread_t id = pthread_self();
   _ht_task_t* task = _ht_create_task();
   _ht_add_task_ref(task);
   task->caller = pth_self();
   getcontext(&(task->task_start_point));
   if(id == pthread_self())
   {
      //caller thread
      pth_event_t e = pth_event(PTH_EVENT_FUNC, 
                                _ht_task_status_check_func, 
                                task, pth_time(0, 50));
      _ht_taskqueue.push(task);
      pth_wait(e);
      return 0;
   }
   else
   {
      return 1;
   }
}

int ht_bg_exec_end()
{
   _ht_worker_ctx_t * worker_ctx = NULL;
   worker_ctx = (_ht_worker_ctx_t*)pthread_getspecific(_ht_worker_ctx_key);
   if(worker_ctx)
   {
      _ht_task_t *task = worker_ctx->curr_task;  
      getcontext(&task->caller->mctx.uc);
   }
   worker_ctx = (_ht_worker_ctx_t*)pthread_getspecific(_ht_worker_ctx_key); 
   if(NULL == worker_ctx)
   {
      //do nothing
      return 0;
   }
   else
   {
      //worker thread
      _ht_task_t *task = worker_ctx->curr_task;  
      memcpy(task->caller->stack, 
             task->task_exec_stack, task->caller->stacksize);
      task->caller->mctx.uc.uc_mcontext.gregs[REG_EBP] = 
              (greg_t)task->caller->stack 
              + task->caller->mctx.uc.uc_mcontext.gregs[REG_EBP] 
              - (greg_t)task->task_exec_stack;
      task->caller->mctx.uc.uc_mcontext.gregs[REG_ESP] = 
              (greg_t)task->caller->stack 
              + task->caller->mctx.uc.uc_mcontext.gregs[REG_ESP] 
              - (greg_t)task->task_exec_stack;

      worker_ctx->curr_task->finished = true;

      _ht_rm_task_ref(worker_ctx->curr_task);
      setcontext(&worker_ctx->worker_resume_ctx);
   }
}

int ht_init(int num_worker)
{
   pthread_key_create(&_ht_worker_ctx_key, NULL);
   pthread_t t;
   for(int i = 0; i < num_worker; i++)
   {
      pthread_mutex_lock(&worker_start_mutex);
      pthread_create(&t, NULL, _ht_async_worker, NULL);
      pthread_cond_wait(&worker_cond_started, &worker_start_mutex); //wait for thread started.
      pthread_mutex_unlock(&worker_start_mutex);
   }
   pth_init();
}



#include <stdio.h>
#include <ucontext.h>
#include <unistd.h>
#include <pthread.h>
#include <unistd.h>
#include "queue.h"
#include "pth.h"
#include "pth_p.h"
#include <memory.h>
struct Task 
{
   ucontext_t task_ctx;
   pth_t th;
   int status; // 0 - onging, 1 - finished.
};

Queue<Task *> g_taskqueue(128);

pthread_key_t _tls_key;

void* f1(void *)
{
   int counter = 0;
   for(;;){
      counter ++;
      if( counter % 90000000 == 0)
      {
         //printf("in f1 counter %d\n", counter);
         counter = counter % 90000000000;
         pth_yield(NULL);
      }
   }
}

int checker(void * status)
{
   int s = *((int*) status);
   return s;
}

#define START_BLOCKING_IO_BLOCK() { \
   Task * task = new Task(); \
   pthread_t pthreadid = pthread_self(); \
   task->status = 0; \
   task->th = pth_self(); \
   getcontext(&(task->task_ctx)); \
   if(pthreadid == pthread_self()) \
   { \
      pth_event_t e = pth_event(PTH_EVENT_FUNC, checker, &(task->status), pth_time(0, 500)); \
      g_taskqueue.push(task); \
      pth_wait(e); \
      delete task; \
   }else{ 

#define END_BLOCKING_IO_BLOCK() } \
   task->status = 1; \
   ucontext_t * worker_ctx = (ucontext_t*)pthread_getspecific(_tls_key); \
   setcontext(worker_ctx); \
   }


void* f2(void *)
{
   for(;;){
      START_BLOCKING_IO_BLOCK()
        printf("seperate thread in f2, start to do blocking stuff. thread id %d\n", pthread_self());
        sleep(1);
        printf("f2 done.thread id %d\n", pthread_self());
      END_BLOCKING_IO_BLOCK() 
      printf("f2 finished blocing io. thread id %d\n", pthread_self());
   }
}

void * f3(void*)
{
   for(;;){
      START_BLOCKING_IO_BLOCK()
        printf("seperate thread in f3, start to do blocking stuff. thread id %d\n", pthread_self());
        sleep(1);
        printf("f3 done.thread id %d\n", pthread_self());
      END_BLOCKING_IO_BLOCK() 
      printf("f3 finished blocing io. thread id %d\n", pthread_self());
   }
}

void * async_worker(void *id)
{
   ucontext_t worker_ctx;
   pthread_setspecific(_tls_key, &worker_ctx);
   char *stack = (char*) malloc(256 * 1024);
   for(;;)
   {
      Task * task = g_taskqueue.pop();
      ucontext tmp = task->task_ctx;
      memcpy(stack, task->th->stack, task->th->stacksize);
      tmp.uc_mcontext.gregs[REG_RBP] = (greg_t)stack + (task->task_ctx.uc_mcontext.gregs[REG_RBP] - (greg_t)task->th->stack);
      tmp.uc_mcontext.gregs[REG_RSP] = (greg_t)stack + (task->task_ctx.uc_mcontext.gregs[REG_RSP] - (greg_t)task->th->stack);
      swapcontext(&worker_ctx, &tmp);
   }
}

int main(int argc, const char *argv[])
{

   pthread_t  t1;
   pthread_key_create(&_tls_key, NULL);
   pthread_create(&t1, NULL, async_worker, NULL);
   pthread_create(&t1, NULL, async_worker, NULL);
   sleep(3);
   
   pth_init();
   pth_attr_t attr = pth_attr_new();
   pth_attr_set(attr, PTH_ATTR_NAME, "f1");
   pth_attr_set(attr, PTH_ATTR_STACK_SIZE, 256*1024);
   pth_attr_set(attr, PTH_ATTR_JOINABLE, TRUE);
   pth_t pf1 = pth_spawn(attr, f1, NULL);
   pth_t pf2 = pth_spawn(attr, f2, NULL);
   pth_t pf3 = pth_spawn(attr, f3, NULL);

   pth_join(pf1, NULL);
   pthread_join(t1, NULL);
   return 0;
}

#include <stdio.h>
#include "pth.h"
#include "ht.h"
#include <unistd.h>
#include <pthread.h>
void* 
f1(void *)
{
   printf("enter f1\n");
   int counter = 0;
   for(;;){
      counter ++;
      if( counter % 990000000 == 0)
      {
         printf("in f1 counter %d\n", counter);
         counter = counter % 90000000000;
         pth_yield(NULL);
      }
   }
}

void* 
f2(void *)
{
   for(;;){
      printf("f2 ht_bg_exec_begin(), thread id %d\n", pthread_self());
     ht_bg_exec_begin();
        printf("seperate thread in f2, start to do blocking stuff. thread id %d\n", pthread_self());
        sleep(1);
        printf("f2 done.thread id %d\n", pthread_self());
     ht_bg_exec_end(); 
      printf("f2 ht_bg_exec_end(). thread id %d\n", pthread_self());
   }
}

void * 
f3(void*)
{
   for(;;){
      ht_bg_exec_begin();
        printf("seperate thread in f3, start to do blocking stuff. thread id %d\n", pthread_self());
        sleep(1);
        printf("f3 done.thread id %d\n", pthread_self());
      ht_bg_exec_end(); 
      printf("f3 finished blocing io. thread id %d\n", pthread_self());
   }
}

int 
main(int argc, const char *argv[])
{

   ht_init(1);
   pth_attr_t attr = pth_attr_new();
   pth_attr_set(attr, PTH_ATTR_NAME, "f1");
   pth_attr_set(attr, PTH_ATTR_STACK_SIZE, 256*1024);
   pth_attr_set(attr, PTH_ATTR_JOINABLE, TRUE);
 //  pth_t pf1 = pth_spawn(attr, f1, NULL);
   pth_t pf2 = pth_spawn(attr, f2, NULL);
 //  pth_t pf3 = pth_spawn(attr, f3, NULL);

   pth_join(pf2, NULL);
   return 0;
}

#include "queue.h"
#include <stdio.h>

int main()
{
   printf("start queue test\n");
   struct queue_t* q = queue_create();
   printf("push a string\n");
   queue_push(q, "hello");
   printf("queue size %d\n", queue_size(q));
   printf("pop a string\n");
   queue_pop(q);
   printf("queue size %d\n", queue_size(q));
}

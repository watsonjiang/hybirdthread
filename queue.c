#include "queue.h"
#include <stdlib.h>
#include <pthread.h>

struct node_t
{
   void * data;
   struct node_t * next;
};

struct queue_t
{
   int size;
   struct node_t * front;
   pthread_mutex_t lock;
   pthread_cond_t  cond_not_empty;
   unsigned int ref_count;
};

struct queue_t* 
queue_create()
{
   struct queue_t * q = (struct queue_t *)malloc(sizeof(struct queue_t));
   q->size = 0;
   q->front = NULL;
   pthread_mutex_init(&q->lock, NULL);
   pthread_cond_init(&q->cond_not_empty, NULL);
   q->ref_count = 0;
}

int
queue_addref(struct queue_t* q)
{
   pthread_mutex_lock(&q->lock);
   q->ref_count ++;
   pthread_mutex_unlock(&q->lock);
}

//only reclaim the resources allocated by us.
int 
queue_rmref(struct queue_t* q)
{
   pthread_mutex_lock(&q->lock);
   q->ref_count --;
   if(q->ref_count == 0)
   {
      struct node_t * p = q->front;
      while(p != NULL)
      {
         struct node_t* n = p;
         p = p->next;
         free(n);
      }
      free(q);
   }
   pthread_mutex_unlock(&q->lock);
   return 0;
}

int 
queue_push(struct queue_t* q, void* obj)
{
   struct node_t * n = (struct node_t *)malloc(sizeof(struct node_t));
   n->data = obj;
   n->next = q->front;
   pthread_mutex_lock(&q->lock);
   q->front = n;
   q->size++;
   pthread_mutex_unlock(&q->lock);
} 
      
void* 
queue_pop(struct queue_t* q)
{
   pthread_mutex_lock(&q->lock);
   if(q->size == 0)
   {
      pthread_cond_wait(&q->cond_not_empty, &q->lock);
   }
   struct node_t* t = q->front;
   while(t->next != NULL)
   {
      t = t->next;
   }
   void * data = t->data;
   struct node_t* r = q->front;
   while(r->next != t)
   {
      r = r->next;
   }
   r->next = NULL;
   q->size--;
   free(t);
   pthread_mutex_unlock(&q->lock);
   return data;
}

unsigned int 
queue_size(struct queue_t* q)
{
   return q->size;
}

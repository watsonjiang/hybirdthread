/*
 * task queue implementation.
 * enqueue blocks when queue is full.
 * dequeue blocks when queue is empty. 
 */
#include "ht_p.h"
/* initialize the queue. */
int 
ht_tqueue_init(ht_tqueue_t * q, int size)
{
   q->q_list = (ht_t*) malloc(sizeof(ht_t) * size);
   q->q_size = size;
   pthread_cond_init(&q->q_not_full, NULL);
   pthread_cond_init(&q->q_not_empty, NULL);
   pthread_mutex_init(&q->q_lock, NULL);
   q->q_head = q->q_rear = 0;
   return 0;
}

/* return the elements in the queue. */
unsigned int 
ht_tqueue_elements(ht_tqueue_t * q)
{
   int n = q->q_head - q->q_rear;
   if (n < 0)
      return -n;
   return n;
}

int
ht_tqueue_enqueue(ht_tqueue_t * q, ht_t t)
{
   pthread_mutex_lock(&q->q_lock);
   if(ht_tqueue_elements(q) == q->q_size)
      pthread_cond_wait(&q->q_not_full, &q->q_lock);
   q->q_list[q->q_head] = t;
   q->q_head = (q->q_head + 1) % q->q_size;
   pthread_cond_signal(&q->q_not_empty);
   pthread_mutex_unlock(&q->q_lock);
   return 0;
}

ht_t
ht_tqueue_dequeue(ht_tqueue_t * q)
{
   pthread_mutex_lock(&q->q_lock);
   if(ht_tqueue_elements(q) == 0)
      pthread_cond_wait(&q->q_not_empty, &q->q_lock);
   ht_t r = q->q_list[q->q_rear];
   q->q_rear = (q->q_rear + 1) % q->q_size;
   pthread_cond_signal(&q->q_not_full);
   pthread_mutex_unlock(&q->q_lock);
   return r;
}

void
ht_tqueue_free(ht_tqueue_t * q)
{
   pthread_cond_destroy(&q->q_not_full);
	pthread_cond_destroy(&q->q_not_empty);
	pthread_mutex_destroy(&q->q_lock);
	free(q->q_list);
	q->q_list = NULL;
}

#ifndef __QUEUE_H
#define __QUEUE_H
/**
 * A LIFO queue implementation in C.
 * based on link list. 
 * will block on pthread cond var when pop an empty queue
 */
struct queue_t;

struct queue_t* queue_create();
int queue_destroy(struct queue_t* queue);
int queue_push(struct queue_t* queue, void* obj);
void* queue_pop(struct queue_t* queue);
unsigned int queue_size(struct queue_t* queue);

#endif //__QUEUE_H

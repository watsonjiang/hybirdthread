#include "ht_p.h"

/* test enqueue, elements, dequeue */
void 
test1()
{
	ht_tqueue_t testQ;
	ht_tqueue_init(&testQ, 3);
	struct ht_st t;
   ht_tqueue_enqueue(&testQ, &t);
	if(1 != ht_tqueue_elements(&testQ))
	{
		printf("error: ht_tqueue_elements did not return expected result.\n");
		exit(1);
	}
   ht_t r = ht_tqueue_dequeue(&testQ);
	if(r != &t)
	{
		printf("error: ht_tqueue_dequeue did not return expected result.\n");
		exit(1);
	}
   ht_tqueue_free(&testQ);
}

/* test block on empty */
static 
void *
test2_worker(void * argv)
{
   ht_tqueue_t* q = (ht_tqueue_t*)argv;
	sleep(3);
	ht_tqueue_enqueue(q, NULL);
	return NULL;
}

void 
test2()
{
   ht_tqueue_t q;
	ht_tqueue_init(&q, 3);
	pthread_t t;
   time_t start, end;
	time(&start);
	pthread_create(&t, NULL, test2_worker, (void*) &q);
   ht_tqueue_dequeue(&q);
	time(&end);
	if(end - start < 3)
	{
		printf("error: ht_tqueue_dequeue did not block when queue is empty\n");
		exit(1);
	}
}

/* test block on full */
static 
void *
test3_worker(void * argv)
{
   ht_tqueue_t* q = (ht_tqueue_t*)argv;
	sleep(3);
	ht_tqueue_dequeue(q);
	return NULL;
}


void
test3()
{
	ht_tqueue_t q;
	ht_tqueue_init(&q, 2);
	ht_tqueue_enqueue(&q, NULL);
   ht_tqueue_enqueue(&q, NULL);
	pthread_t t;
   time_t start, end;
	time(&start);
	pthread_create(&t, NULL, test3_worker, (void*) &q);
   ht_tqueue_enqueue(&q, NULL);
	time(&end);
	if(end - start < 3)
	{
		printf("error: ht_tqueue_enqueue did not block when queue is full\n");
		exit(1);
	}
}

int 
main()
{
   test1();
	test2();
	test3();
	return 0;
}

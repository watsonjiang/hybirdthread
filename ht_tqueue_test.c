#include "ht_p.h"
#include "ht_test.h"
/* test enqueue, elements, dequeue */
void 
test1()
{
   ht_tqueue_t testQ;
   ht_tqueue_init(&testQ, 3);
   struct ht_st t;
   ht_tqueue_enqueue(&testQ, &t);
   HT_TEST_ASSERT(1 == ht_tqueue_elements(&testQ),
                  "ht_tqueue_elements did not return expected result.");
   ht_t r = ht_tqueue_dequeue(&testQ);
   HT_TEST_ASSERT(r == &t,
                  "ht_tqueue_dequeue did not return expected result.");
   ht_tqueue_destroy(&testQ);
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
   HT_TEST_ASSERT(0 == ht_tqueue_elements(&q), 
			         "ht_tqueue_elements did not return expected value.");
   ht_tqueue_dequeue(&q);
   time(&end);
   HT_TEST_ASSERT((end - start >= 3),
                  "error: ht_tqueue_dequeue did not block when queue is empty");
   ht_tqueue_destroy(&q);
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
   ht_tqueue_init(&q, 1);
   ht_tqueue_enqueue(&q, NULL);
   pthread_t t;
   time_t start, end;
   time(&start);
   pthread_create(&t, NULL, test3_worker, (void*) &q);
	HT_TEST_ASSERT(1 == ht_tqueue_elements(&q), 
			         "ht_tqueue_elements did not return expected value.");
   ht_tqueue_enqueue(&q, NULL);
   time(&end);
   HT_TEST_ASSERT(end - start >= 3, 
			         "ht_tqueue_enqueue did not block when queue is full.");
   ht_tqueue_destroy(&q);
}

void test4()
{
   ht_tqueue_t q;
   ht_tqueue_init(&q, 2);
   struct ht_st t;
   ht_tqueue_enqueue(&q, &t);
   HT_TEST_ASSERT(1 == ht_tqueue_elements(&q), "");
   HT_TEST_ASSERT(&t == ht_tqueue_dequeue(&q), "");
   HT_TEST_ASSERT(0 == ht_tqueue_elements(&q), "");
   ht_tqueue_enqueue(&q, &t);
   HT_TEST_ASSERT(1 == ht_tqueue_elements(&q), "");
   HT_TEST_ASSERT(&t == ht_tqueue_dequeue(&q), "");
   HT_TEST_ASSERT(0 == ht_tqueue_elements(&q), "");
    ht_tqueue_enqueue(&q, &t);
   HT_TEST_ASSERT(1 == ht_tqueue_elements(&q), "");
   HT_TEST_ASSERT(&t == ht_tqueue_dequeue(&q), "");
   HT_TEST_ASSERT(0 == ht_tqueue_elements(&q), "");
}

int 
main()
{
   test1();
   test2();
   test3();
   test4();
   return 0;
}

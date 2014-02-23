#include "ht_p.h"
#include "ht_test.h"

void 
test1()
{
   pthread_t tid = pthread_self();
   ht_hand_out();
   HT_TEST_ASSERT(tid != pthread_self(), 
                  "ht_hand_out() did not dispatch task to worker.");
   ht_get_back();
   HT_TEST_ASSERT(tid == pthread_self(),
                  "ht_get_back() did not get back task from worker."); 
}

int
main()
{
   ht_init();
   test1();
   ht_kill();
   return 0;
}

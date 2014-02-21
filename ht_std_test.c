#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include "ht.h"
#include "ht_test.h"

static 
void *
t1_func(void *arg)
{
    int i;
    long val;

    val = (long)arg;
    HT_TEST_ASSERT(val == 123, "arg value not as expected.");
    for (i = 0; i < 100; i++) {
        val += 10;
        ht_yield(NULL);
    }
    return (void *)val;
}

static void *t2_func(void *arg)
{
    long val;
    ht_t tid;
    void *rval;
    int rc;

    val = (long)arg;
    if (val < 9) {
        val++;
        tid = ht_spawn(HT_ATTR_DEFAULT, t2_func, (void *)(val));
        HT_TEST_ASSERT(tid != NULL, "")
        rc = ht_join(tid, &rval);
        HT_TEST_ASSERT(rc != FALSE, "")
        rval = (void *)((long)arg * (long)rval);
    }
    else
        rval = arg;
    return rval;
}

int main(int argc, char *argv[])
{
    /*=== TESTING GLOBAL LIBRARY API ===*/
    {
        int version;

        /* Fetching library version */
        version = ht_version();
        HT_TEST_ASSERT(version != 0x0, 
                       "ht_version did not return expected value.");
    }

    /*=== TESTING BASIC OPERATION ===*/
    {
        int rc;

        /* Initializing Pth system (spawns scheduler and main thread) */
        rc = ht_init();
        HT_TEST_ASSERT(rc != FALSE, "ht_init failed.");
        /* Killing Pth system for testing purposes */
        ht_kill();
        /* Re-Initializing Pth system */
        rc = ht_init();
        HT_TEST_ASSERT(rc != FALSE, "ht_init failed.");
    }

    /*=== TESTING BASIC THREAD OPERATION ===*/
    {
        ht_attr_t attr;
        ht_t tid;
        void *val;
        int rc;

        /* Creating attribute object */
        attr = ht_attr_new();
        HT_TEST_ASSERT(attr != NULL, "ht_attr_new returned NULL");
        rc = ht_attr_set(attr, HT_ATTR_NAME, "test1");
        HT_TEST_ASSERT(rc != FALSE, "ht_attr_set failed on HT_ATTR_NAME");
        rc = ht_attr_set(attr, HT_ATTR_PRIO, HT_PRIO_MAX);
        HT_TEST_ASSERT(rc != FALSE, "ht_attr_set failed on HT_ATTR_PRIO");

        /* "Spawning thread\n" */
        tid = ht_spawn(attr, t1_func, (void *)(123));
        HT_TEST_ASSERT(tid != NULL, "ht_spawn failed.");
        ht_attr_destroy(attr);

        /* Joining thread */
        rc = ht_join(tid, &val);
        HT_TEST_ASSERT(rc != FALSE, "ht_join failed.");
        HT_TEST_ASSERT(val == (void *)(1123), 
                       "ht_join did not return expected value.");
    }
    
    /*=== TESTING NESTED THREAD OPERATION ===*/
    {
        ht_t tid;
        void *val;
        int rc;

        /* Spawning thread 1 */
        tid = ht_spawn(HT_ATTR_DEFAULT, t2_func, (void *)(1));
        HT_TEST_ASSERT(tid != NULL, "ht_spawn failed.");

        rc = ht_join(tid, &val);
        /* Joined thread 1 */
        HT_TEST_ASSERT(rc != FALSE, "ht_join failed.");
        HT_TEST_ASSERT(val == (void *)(1*2*3*4*5*6*7*8*9),
                       "ht_join did not return expected value.");
    }

    ht_kill();
    exit(0);
}


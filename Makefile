
CFLAGS=  -g \
         -fpic \
         -Wall \
         -Werror \
         -Wfatal-errors 
         

LDFLAGS=-g \
        -shared 

CC=gcc 

OBJS=ht_errno.o ht_string.o ht_debug.o ht_util.o ht_attr.o ht_time.o ht_pqueue.o \
     ht_tcb.o ht_sched.o ht_data.o ht_cancel.o ht_clean.o ht_event.o ht_high.o \
     ht_lib.o ht_mctx.o ht_msg.o ht_ring.o ht_sync.o ht_uctx.o ht_tqueue.o \
     ht_worker.o

BINS=libht.so

TEST_BINS=ht_tqueue_test ht_worker_test ht_std_test

all: $(BINS)

test: $(TEST_BINS)
	for i in ${TEST_BINS}; do echo -e "$$i.....\c";export LD_LIBRARY_PATH=.;./$$i; if [ $$? == 0 ]; then echo "done"; else exit 1; fi; done

libht.so: $(OBJS)
	gcc $(LDFLAGS) -o $@ $^   

ht_tqueue_test: libht.so ht_tqueue_test.o
	gcc ${CFLAGS} -L. -lht -lpthread -o $@ ht_tqueue_test.o

ht_worker_test: libht.so ht_worker_test.o 
	gcc ${CFLAGS} -L. -lht -lpthread -o $@ ht_worker_test.o

ht_std_test: libht.so ht_std_test.o
	gcc ${CLAGS} -L. -lht -lpthread -o $@ ht_std_test.o

clean:
	rm -rf $(BINS) $(TEST_BINS) *.o

.PHONY: clean all test

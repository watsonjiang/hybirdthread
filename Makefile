
CFLAGS=  -g \
         -fpic \
         -Wall \
         -Werror \
         -Wfatal-errors
         

LDFLAGS=-g \
        -shared \
        -lpthread 

CC=gcc 

OBJS=ht_errno.o ht_string.o ht_debug.o ht_util.o ht_attr.o ht_time.o ht_pqueue.o \
     ht_tcb.o ht_sched.o ht_data.o ht_cancel.o ht_clean.o ht_event.o ht_high.o \
     ht_lib.o ht_mctx.o ht_msg.o ht_ring.o ht_sync.o ht_uctx.o

BINS=libht.so

TEST_BINS=queue_test ht_test

all: $(BINS)

libht.so: $(OBJS)
	gcc $(LDFLAGS) -o $@ $^   

clean:
	rm -rf $(BINS) *.o

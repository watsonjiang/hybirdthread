
CFLAGS=  -g \
         -fpic \
         -Wall \
         -Wfatal-errors
         

LDFLAGS=-g \
        -shared \
        -lpthread 

CC=gcc 

OBJS=ht_errno.o ht_string.o ht_debug.o ht_util.o ht_attr.o ht_time.o ht_pqueue.o \
     ht_tcb.o ht_sched.o

BINS=libht.so

TEST_BINS=queue_test ht_test

all: $(BINS)

libht.so: $(OBJS)


clean:
	rm -rf a.out *.o

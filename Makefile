
CXXFLAGS=-g \
         -fpic \
         -Wall \
         -Werror \
         -Wfatal-errors
         

LDFLAGS=-g \
        -shared \
        -lpthread 

CC=gcc 

OBJS=ht_errno.o ht_debug.o

BINS=libht.so

TEST_BINS=queue_test ht_test

all: $(BINS)

libht.so: $(OBJS)


clean:
	rm -rf a.out *.o

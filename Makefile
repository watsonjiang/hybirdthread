
CXXFLAGS=-g \
         -I./pth \
         -fpermissive

LDFLAGS=-g \
        -lpthread \
        -L./pth/.libs -lpth

OBJS=main.o

BINS=a.out

all: $(BINS)


$(BINS):$(OBJS)   
	g++ $(LDFLAGS) -o $@ $^ 

clean:
	rm -rf a.out *.o

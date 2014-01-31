
CXXFLAGS=-g \
         -I./pth \

LDFLAGS=-g \
        -lpthread \
        -lpth

OBJS=main.o ht.o

BINS=a.out

all: $(BINS)


$(BINS):$(OBJS)   
	g++ $(LDFLAGS) -o $@ $^ 

clean:
	rm -rf a.out *.o

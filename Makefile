CFLAGS=-std=c11 -g -static
SRCS = $(filter-out tests.c test-extern.c, $(wildcard *.c))
OBJS=$(SRCS:.c=.o)

all: chibicc test test-gen2 clean

chibicc: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): chibicc.h

chibicc-gen2: chibicc $(SRCS) chibicc.h
	./self.sh

extern.o: tests-extern.c
	gcc -xc -c -o extern.o tests-extern.c

test: chibicc extern.o
			./chibicc tests.c > tmp.s
			gcc -static -o tmp tmp.s extern.o
			./tmp

test-gen2: chibicc-gen2 extern.o
			./chibicc-gen2 tests.c > tmp.s
			gcc -static -o tmp tmp.s extern.o
			./tmp

eight-queen: chibicc
			./chibicc examples/nqueen.c > tmp.s
			gcc -static -o tmp tmp.s
			./tmp

clean:
	rm -rf chibicc chibicc-gen* *.o *~ tmp*

.PHONY: test clean

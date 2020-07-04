CFLAGS=-std=c11 -g -static
SRCS = $(filter-out tests.c, $(wildcard *.c))
OBJS=$(SRCS:.c=.o)

all: chibicc test clean

chibicc: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): chibicc.h

test: chibicc
			./chibicc tests.c > tmp.s
			echo 'int ext1; int *ext2; int char_fn() { return 257; }' \
				 'int static_fn() { return 5; }' | \
				gcc -xc -c -o tmp2.o -
			gcc -static -o tmp tmp.s tmp2.o
			./tmp

eight-queen: chibicc
			./chibicc examples/nqueen.c > tmp.s
			gcc -static -o tmp tmp.s
			./tmp

clean:
	rm -f chibicc *.o *~ tmp*

.PHONY: test clean

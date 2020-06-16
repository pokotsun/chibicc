CFLAGS=-std=c11 -g -static
SRCS = $(filter-out tests.c, $(wildcard *.c))
OBJS=$(SRCS:.c=.o)

all: chibicc test clean

chibicc: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): chibicc.h

test: chibicc
			./chibicc tests.c > tmp.s
			echo 'int char_fn() { return 257; }' | gcc -xc -c -o tmp2.o -
			gcc -static -o tmp tmp.s tmp2.o
			./tmp

clean:
	rm -f chibicc *.o *~ tmp*

.PHONY: test clean

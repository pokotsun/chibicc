CFLAGS=-std=c11 -g -static

all: chibicc test clean

chibicc: chibicc.c

test: chibicc
			./test.sh

clean:
	rm -f chibicc *.o *~ tmp*

.PHONY: test clean

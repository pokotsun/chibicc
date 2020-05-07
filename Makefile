CFLAGS=-std=c11 -g -static
SRCS=$(wildcard *.c)
OBJS=$(SRCS:.c=.o)

all: chibicc test clean

chibicc: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(OBJS): chibicc.h

test: chibicc
			./test.sh

clean:
	rm -f chibicc *.o *~ tmp*

.PHONY: test clean

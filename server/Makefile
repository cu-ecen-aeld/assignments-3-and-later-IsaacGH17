CROSS_COMPILE ?=
CC = ${CROSS_COMPILE}gcc
default: all
all: aesdsocket

aesdsocket: aesdsocket.o
	$(CC) -Wall -Werror -g -o $@ $^

aesdsocket.o: aesdsocket.c aesdsocket.h
	$(CC) -Wall -Werror -c -o $@ aesdsocket.c

clean:
	rm -f aesdsocket *.o
.PHONY: all clean

CROSS_COMPILE ?=
CC = $(CROSS_COMPILE)gcc
all: writer
writer: writer.c
	$(CC) -Wall -Werror -g writer.c -o writer
clean:
	rm -f writer *.o
.PHONY: all clean

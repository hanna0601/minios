CC     = gcc
CFLAGS = -Wall -Wextra -Werror -g

.PHONY: all clean

all: test_queue

test_queue: test_queue.o queue.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o test_queue

CC     = cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -pedantic

all: schedule tests

schedule: main.c greedy.c interval.h
	$(CC) $(CFLAGS) -o $@ main.c greedy.c

tests: tests.c greedy.c interval.h
	$(CC) $(CFLAGS) -o $@ tests.c greedy.c

test: tests
	./tests

clean:
	rm -f schedule tests

.PHONY: all test clean

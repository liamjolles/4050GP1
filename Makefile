CC     = cc
CFLAGS = -std=c11 -O2 -Wall -Wextra -pedantic

all: schedule tests validate search

schedule: main.c greedy.c exact.c interval.h
	$(CC) $(CFLAGS) -o $@ main.c greedy.c exact.c

tests: tests.c greedy.c exact.c gen.c interval.h gen.h
	$(CC) $(CFLAGS) -o $@ tests.c greedy.c exact.c gen.c

validate: validate.c greedy.c exact.c gen.c interval.h gen.h
	$(CC) $(CFLAGS) -o $@ validate.c greedy.c exact.c gen.c -lm

search: search.c greedy.c exact.c gen.c interval.h gen.h
	$(CC) $(CFLAGS) -o $@ search.c greedy.c exact.c gen.c

test: tests
	./tests

part3: search
	mkdir -p results
	./search

# Parts 4 and 5 come from the same run
part4 part5: validate
	mkdir -p results
	./validate

part6: validate
	mkdir -p results
	./validate time

clean:
	rm -f schedule tests validate search

.PHONY: all test part3 part4 part5 part6 clean

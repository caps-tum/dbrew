# can be overwritten
CC=gcc

.PHONY: all test store clean

all: test

test:
	CC=$(CC) ./test.py

store:
	./test.py --store

clean:
	rm -f *.o cases/*/*.o cases/*/*.out *~

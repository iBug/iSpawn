CC := gcc

.PHONY: all clean

all: ispawn

ispawn: main.o
	${CC} -o $@ $^

main.o: main.c
	${CC} -c -o $@ $^

clean:
	rm -f ispawn *.o

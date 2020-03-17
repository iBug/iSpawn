CC := gcc

.PHONY: all clean

all: ispawn

ispawn: main.o util.o
	${CC} -o $@ $^

%.o: %.c
	${CC} -c -o $@ $^

clean:
	rm -f ispawn *.o

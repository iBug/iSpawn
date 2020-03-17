CC := gcc

.PHONY: all clean

all: ispawn

ispawn: main.o util.o cap.o
	${CC} -o $@ $^ -lcap

%.o: %.c
	${CC} -c -o $@ $^

clean:
	rm -f ispawn *.o

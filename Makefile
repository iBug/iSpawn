CC := gcc
CFLAGS ?=

.PHONY: all clean

all: ispawn

ispawn: main.o util.o cap.o fs.o
	${CC} ${CFLAGS} -o $@ $^ -lcap

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $^

clean:
	rm -f ispawn *.o

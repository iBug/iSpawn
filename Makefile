CC := gcc
CFLAGS ?=
LIBS = -lcap -lseccomp

.PHONY: all clean

all: ispawn

ispawn: main.o util.o cap.o fs.o syscall.o syscall_allow.o
	${CC} ${CFLAGS} -o $@ $^ ${LIBS}

%.o: %.c
	${CC} ${CFLAGS} -c -o $@ $^

clean:
	rm -f ispawn *.o

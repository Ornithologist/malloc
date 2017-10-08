CC=gcc
CFLAGS=-g -O3 -fPIC -fno-builtin
CFLAGS_AFT=-lm -lpthread

all: check

default: check

clean:
	rm -rf libmalloc.so malloc.o

lib: libmalloc.so

libmalloc.so: combo.o
	$(CC) $(CFLAGS) -shared -Wl,--unresolved-symbols=ignore-all $< -o $@ $(CFLAGS_AFT)

test1: test1.o
	$(CC) $(CFLAGS) $< -o $@ $(CFLAGS_AFT)

# For every XYZ.c file, generate XYZ.o.
%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@ $(CFLAGS_AFT)

check:	clean libmalloc.so test1
	LD_PRELOAD=`pwd`/libmalloc.so ./test1

dist:
	dir=`basename $$PWD`; cd ..; tar cvf $$dir.tar ./$$dir; gzip $$dir.tar 
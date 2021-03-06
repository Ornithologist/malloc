CC=gcc
CFLAGS=-g -O0 -fPIC -fno-builtin
CFLAGS_AFT=-lm -lpthread

all: check

default: check

clean:
	rm -rf libmalloc.so *.o test1

lib: libmalloc.so

libmalloc.so: malloc.o free.o calloc.o realloc.o malloc_stats.o mallinfo.o
	$(CC) -g -o0 -shared -Wl,--unresolved-symbols=ignore-all malloc.o free.o calloc.o realloc.o malloc_stats.o mallinfo.o -o libmalloc.so $(CFLAGS_AFT)

test1: test1.o
	$(CC) $(CFLAGS) $< -o $@ $(CFLAGS_AFT)

gdb: libmalloc.so test1
	gdb --args env LD_PRELOAD=./libmalloc.so ./test1

# For every XYZ.c file, generate XYZ.o.
%.o: %.c
	$(CC) $(CFLAGS) $< -c -o $@ $(CFLAGS_AFT)

check:	clean libmalloc.so test1
	LD_PRELOAD=`pwd`/libmalloc.so ./test1

dist:
	dir=`basename $$PWD`; cd ..; tar cvf $$dir.tar ./$$dir; gzip $$dir.tar 
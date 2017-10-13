#include <assert.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "common.h"

/*
 * returns the main thread arena information
 * arenas from other threads are excluded
 */
struct mallinfo_t __lib_mallinfo() {
    mallinfo_t *main_mallinfo_p = (mallinfo_t *)((char *)main_thread_arena_p + sizeof(arena_h_t));
    mallinfo_t main_mallinfo = (*main_mallinfo_p);
    return main_mallinfo;
}
struct mallinfo_t mallinfo() __attribute__((weak, alias("__lib_mallinfo")));
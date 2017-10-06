#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include "common.h"

static int arena_count = 1;
static int processor_count = 1;
static int page_size = 4096;

static __thread arena_meta_t* ts_arena_ptr;
static __thread pthread_key_t arena_key;
static thread_meta_t main_thread = { 0 };
static pthread_mutex_t malloc_thread_init_lock = PTHREAD_MUTEX_INITIALIZER;

void *malloc(size_t size)
{
    block_meta_t *block_p;
    size_t size_needed = size + sizeof(block_meta_t);
    uint8_t order_needed = SIZE_TO_ORDER(size_needed);
    if (order_needed > MAX_ORDER) {
        // use mmap
    } else {
        // use sbrk
        if ((block_p = alloc_by_order(ts_arena_ptr, order_needed)) != NULL)
            block_p->allocated = 1;
    }
    return (void *) block_p->block_data.data;
}
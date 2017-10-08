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

#ifndef __COMMON_H__
#define __COMMON_H__

#define BASE 2 // 2 bytes
#define MAX_BIN 9
#define MIN_ORDER 5 // 32 bytes
#define MAX_ORDER 12 // 4096 bytes
#define HEAP_PAGE_SIZE 4096

#define SIZE_TO_ORDER(size) ( ceil( (log(size) / log(BASE) )) )
#define ORDER_TO_BIN_INDEX(order) (order - MIN_HEAP_BLOCK_SIZE_ORDER)

typedef struct _block_header {
	uint8_t order;
    void* addr;
    struct _block_header* next;
} block_h_t;

typedef struct _areana_header {
    pthread_mutex_t lock;
	arena_status_t arena_status;
	block_h_t* bins[MAX_BINS];
	size_t bin_counts[MAX_BINS];
	size_t total_alloc_req;
	size_t total_free_req;
	struct _arena_header* next_arena;
} areana_h_t;

static int no_of_arenas = 1;
static int no_of_processors = 1;
static long sys_page_size = HEAP_PAGE_SIZE;
static bool malloc_initialized = 0;
static __thread areana_h_t* areana_h_t *cur_arena_p;
static __thread pthread_key_t cur_arena_key;
static pthread_mutex_t malloc_thread_init_lock = PTHREAD_MUTEX_INITIALIZER;

static void* initialize_malloc_lib(size_t size, const void* caller);
void release_mmap_blocks(areana_h_t* ar_ptr, block_h_t* block_ptr);

typedef void *(*__hook)(size_t __size, const void *);
__hook        __malloc_hook = (__hook ) initialize_malloc_lib;

extern void *malloc(size_t size);

typedef void (*pthread_atfork_handlers)(void);
typedef void (*thread_exit_handlers)(void*);

#endif
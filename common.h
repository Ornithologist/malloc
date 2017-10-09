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

#define SUCCESS 0
#define FAILURE 1
#define IN_USE 1
#define VACANT 0
#define VALID 0
#define INVALID 1

#define BASE 2  // 2 bytes
#define MAX_BINS 9
#define MIN_ORDER 5   // 32 bytes
#define MAX_ORDER 12  // 4096 bytes
#define HEAP_PAGE_SIZE 4096

#define SIZE_TO_ORDER(size) (ceil((log(size) / log(BASE))))

typedef struct _block_header {
    uint8_t order;
    uint8_t status;
    void *order_base_addr;
    struct _block_header *next;
} block_h_t;

typedef struct _heap_header {
    size_t size;
    void *base_block;
    struct _heap_header *next;
} heap_h_t;

typedef struct _arena_header {
    pthread_mutex_t lock;
    uint16_t no_of_heaps;
    uint8_t status;
    block_h_t *bins[MAX_BINS];
    size_t bin_counts[MAX_BINS];
    size_t total_alloc_req;
    size_t total_free_req;
    void *base_heap;
    struct _arena_header *next;
} arena_h_t;

typedef struct __malloc_metadata {
    arena_h_t arena_ptr;
} mmeta_t;

static int no_of_arenas = 1;
static int no_of_processors = 1;
static long sys_page_size = HEAP_PAGE_SIZE;
static bool malloc_initialized = 0;
static mmeta_t main_thread_metadata = {0};
static __thread arena_h_t *cur_arena_p;
static __thread pthread_key_t cur_arena_key;
static pthread_mutex_t malloc_thread_init_lock = PTHREAD_MUTEX_INITIALIZER;

int initialize_main_arena();
int initialize_thread_arena();
int initialize_new_heap(arena_h_t *ar_ptr);
int validate_addr(arena_h_t *ar_ptr, void *mem_ptr);
void thread_destructor(void *ptr);
void *initialize_malloc_lib(size_t size, const void *caller);
void release_buddy_block(arena_h_t *ar_ptr, block_h_t *block_ptr);
void release_mmap_block(arena_h_t *ar_ptr, block_h_t *block_ptr);
void insert_heap_to_arena(arena_h_t *ar_ptr, heap_h_t *heap_ptr);
void insert_block_to_arena(arena_h_t *ar_ptr, uint8_t bin_index,
                           block_h_t *block_to_insert);
void *divide_block_and_add_to_bins(arena_h_t *ar_ptr, uint8_t bin_index,
                                   block_h_t *mem_block_ptr,
                                   int block_size_order);

block_h_t *find_vacant_block(arena_h_t *ar_ptr, uint8_t bin_index);
block_h_t *allocate_new_block(arena_h_t *ar_ptr, size_t size_order);
block_h_t *find_vacant_buddy(block_h_t *block_ptr);

typedef void *(*__hook)(size_t __size, const void *);
static __hook __malloc_hook = (__hook)initialize_malloc_lib;

extern void *malloc(size_t size);

typedef void (*pthread_atfork_handlers)(void);
typedef void (*thread_exit_handlers)(void *);

#endif
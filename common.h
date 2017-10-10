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

/*
 * struct for a memory block in the buddy system
 * @attri order: the log(size_in_bytes) / log(2)
 * @attri status: IN_USE (returned by malloc) / VACANT (free to use)
 * @attri order_base_addr: the base address of the parent block
 * @attri next: pointer to the cloest next block with the same order
 */
typedef struct _block_header {
    uint8_t order;
    uint8_t status;
    void *order_base_addr;
    struct _block_header *next;
} block_h_t;

/*
 * struct for an integral memory region allocated with sbrk or mmap
 * @attri size: size in bytes, must be multiple of PAGE_SIZE
 * @attri base_block: the starting address of this memory region
 * @attri next: pointer to the next heap_h_t from the same arena
 */
typedef struct _heap_header {
    size_t size;
    void *base_block;
    struct _heap_header *next;
} heap_h_t;

/*
 * struct for per-thread memory arena
 * @attri lock: mutext lock
 * @attri no_of_heaps: number of heaps this arena maintains
 * @attri status: VACANT (no allocation yet) / IN_USE (at least one allocation)
 * @attri bins: array of blocks, each with order 5 to 12, and bigger than 12
 * @attri bin_counts: number of blocks in different order
 * @attri total_alloc_req: number of allocation requests
 * @attri total_free_req: number of free requests
 * @attri base_heap: pointer to the first heap struct belongs to this arena
 */
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

int initialize_main_arena();
int initialize_thread_arena();
int initialize_new_heap(arena_h_t *ar_ptr);
int validate_addr(arena_h_t *ar_ptr, void *mem_ptr);
void thread_destructor(void *ptr);
void *initialize_malloc_lib(size_t size, const void *caller);
void release_buddy_block(arena_h_t *ar_ptr, block_h_t *block_ptr);
void release_mmap_block(arena_h_t *ar_ptr, block_h_t *block_ptr);
void insert_heap_to_arena(arena_h_t *ar_ptr, size_t size, block_h_t *block_ptr);
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

extern int no_of_arenas;
extern int no_of_processors;
extern long sys_page_size;
extern bool malloc_initialized;
// extern mmeta_t main_thread_metadata;
extern __thread arena_h_t *cur_arena_p;
extern __thread heap_h_t *cur_base_heap_p;
extern __thread pthread_key_t cur_arena_key;
static pthread_mutex_t malloc_thread_init_lock = PTHREAD_MUTEX_INITIALIZER;

typedef void (*pthread_atfork_handlers)(void);
typedef void (*thread_exit_handlers)(void *);

#endif
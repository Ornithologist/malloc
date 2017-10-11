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

#define BASE 2          // 2 bytes
#define MAX_BINS 9      // 32 to 4096, plus >4096
#define MIN_ORDER 5     // 32 bytes
#define MAX_ORDER 12    // 4096 bytes
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
 * @attri base_heap: pointer to the first heap struct belongs to this arena
 */
typedef struct _arena_header {
    pthread_mutex_t lock;
    uint16_t no_of_heaps;
    uint8_t status;
    block_h_t *bins[MAX_BINS];
    size_t bin_counts[MAX_BINS];
    void *base_heap;
    struct _arena_header *next;
} arena_h_t;

/*
 * struct for malloc info
 * @attri arena: total number of bytes allocated with mmap/sbrk
 * @attri narenas: number of arenas
 * @attri alloreqs: number of allocation requests
 * @attri freereqs: number of free requests
 * @attri alloreqs: number of allocated blocks
 * @attri freereqs: number of free blocks
 * @attri uordblks: total allocated space in bytes
 * @attri fordblks: total free space in bytes
 */
typedef struct _mallinfo {
    int arena;
    int narenas;
    int alloreqs;
    int freereqs;
    int alloblks;
    int freeblks;
    int uordblks;
    int fordblks;
} mallinfo;

int initialize_main_arena();
int initialize_thread_arena();
int initialize_new_heap(arena_h_t *ar_ptr);
int validate_addr(arena_h_t *ar_ptr, void *mem_ptr);
void thread_destructor(void *ptr);
void *initialize_lib(size_t size, const void *caller);
void initialize_free(void *ptr, const void *caller);
void *initialize_realloc(void *ptr, size_t size, const void *caller);
void *initialize_calloc(size_t nmemb, size_t size, const void *caller);
void release_buddy_block(arena_h_t *ar_ptr, block_h_t *block_ptr);
void release_mmap_block(arena_h_t *ar_ptr, block_h_t *block_ptr);
void link_heap_to_arena(arena_h_t *ar_ptr, size_t size, block_h_t *block_ptr);
void remove_heap_from_arena(arena_h_t *ar_ptr, block_h_t *block_ptr);
void link_block_to_arena(arena_h_t *ar_ptr, uint8_t bin_index,
                           block_h_t *block_to_insert);
void *split_block_to_buddies(arena_h_t *ar_ptr, block_h_t *mem_block_ptr,
                                   int block_size_order);

block_h_t *find_vacant_block(arena_h_t *ar_ptr, uint8_t bin_index);
block_h_t *find_vacant_mmap_block(arena_h_t *ar_ptr, uint8_t size_order);
block_h_t *sbrk_new_block(arena_h_t *ar_ptr, uint8_t size_order);
block_h_t *mmap_new_block(arena_h_t *ar_ptr, uint8_t size_order);
block_h_t *find_vacant_buddy(block_h_t *block_ptr);

typedef void *(*__malloc_hook_t)(size_t size, const void *caller);
typedef void (*__free_hook_t)(void *ptr, const void *caller);
typedef void *(*__realloc_hook_t)(void *ptr, size_t size, const void *caller);
typedef void *(*__calloc_hook_t)(size_t nmemb, size_t size, const void *caller);

void *__lib_malloc(size_t size);
extern void *malloc(size_t size);
extern void *free(void *mem_ptr);
extern void *calloc(size_t nmemb, size_t size);
extern void *realloc(void *ptr, size_t size);

extern long sys_page_size;
extern bool malloc_initialized;
extern mallinfo mallinfo_global;
extern arena_h_t *main_thread_arena_p;
extern __thread mallinfo cur_mallinfo;
extern __thread arena_h_t *cur_arena_p;
extern __thread heap_h_t *cur_base_heap_p;
extern __thread pthread_key_t cur_arena_key;
extern pthread_mutex_t lib_ini_lock;

#endif
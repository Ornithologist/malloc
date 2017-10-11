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

// ini globals
// FIXME: a better way to organise ?
long sys_page_size = HEAP_PAGE_SIZE;
bool malloc_initialized = 0;
arena_h_t *main_thread_arena_p;
pthread_mutex_t lib_ini_lock = PTHREAD_MUTEX_INITIALIZER;
mallinfo mallinfo_global = (mallinfo){0, 0, 0, 0, 0, 0};

// per thread global
__thread heap_h_t *cur_base_heap_p;
__thread arena_h_t *cur_arena_p;
__thread pthread_key_t cur_arena_key;
__thread mallinfo cur_mallinfo = (mallinfo){0, 0, 0, 0, 0, 0};

/*
 * link block to ar_ptr->bins[bin_index]
 * by sorting from smallest addr to largest addr
 */
void link_block_to_arena(arena_h_t *ar_ptr, uint8_t bin_index,
                           block_h_t *block_to_insert)
{
    ar_ptr->bin_counts[bin_index]++;
    block_to_insert->next = NULL;

    if (ar_ptr->bins[bin_index] == NULL) {  // first comer
        ar_ptr->bins[bin_index] = block_to_insert;
    } else if (block_to_insert <
               ar_ptr->bins[bin_index]) {  // link as start tail
        block_to_insert->next = ar_ptr->bins[bin_index];
        ar_ptr->bins[bin_index] = block_to_insert;
    } else {  // link through all existing bins of the same order
        block_h_t *itr = ar_ptr->bins[bin_index], *prev_block = NULL;

        while (itr != NULL && block_to_insert > itr) {
            prev_block = itr;
            itr = itr->next;
        }

        if (itr == NULL) {  // link as end tail
            block_to_insert->next = NULL;
            prev_block->next = block_to_insert;
        } else {  // link in the middle
            block_to_insert->next = prev_block->next;
            prev_block->next = block_to_insert;
        }
    }
}

/*
 * create a heap with size=size, base_block=block_ptr;
 * link the heap to ar_ptr->base_heap by appending to the link list
 */
void link_heap_to_arena(arena_h_t *ar_ptr, size_t size, block_h_t *block_ptr)
{
    heap_h_t *new_heap_ptr, *prev_itr = NULL;
    heap_h_t *itr = (heap_h_t *)ar_ptr->base_heap;
    while (itr != NULL) {
        prev_itr = itr;
        itr = itr->next;
    }
    if (prev_itr == NULL) {
        new_heap_ptr = (heap_h_t *)((char *)ar_ptr + sizeof(arena_h_t));
        new_heap_ptr->size = size;
        new_heap_ptr->base_block = block_ptr;
        new_heap_ptr->next = NULL;
        ar_ptr->base_heap = new_heap_ptr;
    } else {
        new_heap_ptr = (heap_h_t *)((char *)prev_itr + sizeof(heap_h_t));
        new_heap_ptr->size = size;
        new_heap_ptr->base_block = block_ptr;
        new_heap_ptr->next = NULL;
        prev_itr->next = new_heap_ptr;
    }
    return;
}

/*
 * split mem_block_ptr into 2 equal-sized buddies;
 * return first buddy (smaller starting addr) to be used by malloc,
 * reserve second buddy by linking it to arena
 */
void *split_block_to_buddies(arena_h_t *ar_ptr, block_h_t *mem_block_ptr,
                             int block_size_order)
{
    int size = pow(2, block_size_order);
    void *mem_block_1 = mem_block_ptr;
    void *mem_block_2 = (((char *)mem_block_ptr) + (size / 2));

    block_h_t *block_hdr = NULL;
    // buddy 1
    block_hdr = (block_h_t *)mem_block_1;
    block_hdr->status = VACANT;
    block_hdr->order = block_size_order - 1;
    block_hdr->order_base_addr = mem_block_ptr->order_base_addr;
    // buddy 2
    block_hdr = (block_h_t *)mem_block_2;
    block_hdr->status = VACANT;
    block_hdr->order = block_size_order - 1;
    block_hdr->order_base_addr = mem_block_ptr->order_base_addr;
    // add second buddy to arena
    link_block_to_arena(ar_ptr, block_size_order - 1 - MIN_ORDER,
                          mem_block_2);
    return mem_block_1;
}

/*
 * find a block from ar_ptr->bins[bin_index],
 * if not found, check for availability at ar_ptr->bins[bin_index+1]
 * and call @split_block_to_buddies accordingly;
 * repeat the process until a block is found, or reached MAX_ORDER;
 * return NULL when not found
 */
block_h_t *find_vacant_block(arena_h_t *ar_ptr, uint8_t bin_index)
{
    // last bin is for size>4096
    if (bin_index == (MAX_BINS - 1)) {
        return NULL;
    }

    block_h_t *ret_ptr = NULL;

    if (ar_ptr->bins[bin_index] != NULL) {
        ret_ptr = ar_ptr->bins[bin_index];
        ar_ptr->bins[bin_index] = ret_ptr->next;
        ar_ptr->bin_counts[bin_index]--;
    } else {
        block_h_t *block =
            (block_h_t *)find_vacant_block(ar_ptr, bin_index + 1);

        if (block != NULL) {
            ret_ptr = split_block_to_buddies(ar_ptr, block, block->order);
        }
    }

    return ret_ptr;
}

/*
 * find vacant block from ar_ptr->bins[8] (mmapped blocks) 
 * that has order no smaller than size_order;
 * return NULL when not found
 */
block_h_t *find_vacant_mmap_block(arena_h_t *ar_ptr, uint8_t size_order)
{
    block_h_t *prev_itr = NULL, *ret_ptr = NULL,
              *itr = ar_ptr->bins[MAX_BINS - 1];

    while (itr != NULL && itr->order < size_order) {
        prev_itr = itr;
        itr = itr->next;
    }

    if (prev_itr != NULL && itr != NULL) {
        ret_ptr = itr;
        prev_itr->next = itr->next;
        ar_ptr->bin_counts[MAX_BINS - 1]--;
    } else if (prev_itr == NULL && itr != NULL) {
        ret_ptr = itr;
        ar_ptr->bins[MAX_BINS - 1] = itr->next;
        ar_ptr->bin_counts[MAX_BINS - 1]--;
    }

    return ret_ptr;
}

/*
 * sbrk a new heap for ar_ptr; find a vacant block of
 * order size_order and return
 */
block_h_t *sbrk_new_block(arena_h_t *ar_ptr, uint8_t size_order)
{
    if (initialize_new_heap(ar_ptr) == FAILURE) {
        errno = ENOMEM;
        return NULL;
    }
    return find_vacant_block(ar_ptr, size_order - MIN_ORDER);
}

/*
 * mmap a new heap for ar_ptr; return the starting addr 
 */
block_h_t *mmap_new_block(arena_h_t *ar_ptr, uint8_t size_order)
{
    size_t size = pow(2, size_order);
    block_h_t *block_ptr = NULL;
    void *mmapped;

    if ((mmapped = mmap(NULL, size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
        errno = ENOMEM;
        return NULL;
    }

    block_ptr = (block_h_t *)mmapped;
    block_ptr->order = size_order;
    block_ptr->order_base_addr = mmapped;
    block_ptr->status = VACANT;
    block_ptr->next = NULL;
    link_block_to_arena(ar_ptr, (MAX_ORDER - MIN_ORDER), block_ptr);

    // ini heap and link to given pointer
    link_heap_to_arena(ar_ptr, size, block_ptr);
    return block_ptr;
}

// called upon before fork() takes place
void prepare_atfork(void)
{
    pthread_mutex_lock(&lib_ini_lock);
    arena_h_t *itr = main_thread_arena_p;
    while (itr) {
        pthread_mutex_lock(&itr->lock);
        itr = itr->next;
    }
}

// shared by parent and child upon completion of fork()
void oncomplete_atfork(void)
{
    arena_h_t *itr = main_thread_arena_p;
    while (itr) {
        pthread_mutex_unlock(&itr->lock);
        itr = itr->next;
    }
    pthread_mutex_unlock(&lib_ini_lock);
}

/*
 * initialize libmalloc; register pthread_atfork handlers;
 * create first arena in the main thread; 
 */
void *initialize_lib(size_t size, const void *caller)
{
    if (pthread_atfork(prepare_atfork, oncomplete_atfork, oncomplete_atfork))
        return NULL;

    if (initialize_main_arena()) {
        return NULL;
    }

    __malloc_hook = NULL;
    return malloc(size);
}

void thread_destructor(void *ptr) {
    if(ptr == NULL)
        return;

    arena_h_t* ar_ptr = ptr;

    pthread_mutex_lock(&lib_ini_lock);
    pthread_mutex_lock(&ar_ptr->lock);

    arena_h_t *itr = main_thread_arena_p, *prev_itr = NULL;
    while (itr != NULL && itr != ar_ptr) {
        prev_itr = itr;
        itr = itr->next;
    }

    if (itr != NULL && prev_itr != NULL) {
        prev_itr->next = itr->next;
    } else if (itr != NULL && prev_itr == NULL) {
        main_thread_arena_p = itr->next;
    }

    pthread_mutex_unlock(&ar_ptr->lock);
    pthread_mutex_unlock(&lib_ini_lock);
}

/*
 * allocate a memory region of PAGE_SIZE;
 * store the arena_h_t instance and all heap_h_t instances
 * in the current thread to this region
 *
 * FIXME: this limits number of heaps to be less than
 * (4096 - sizeof(arena_h_t)) / sizeof(heap_h_t),
 * find a more scalable solution
 */
int initialize_arena_meta()
{
    int out = SUCCESS;
    if ((cur_arena_p = (arena_h_t *)sbrk(sys_page_size)) == NULL)
        return FAILURE;

    cur_arena_p->status = IN_USE;
    cur_arena_p->no_of_heaps = 0;
    cur_arena_p->next = NULL;
    cur_arena_p->base_heap = NULL;
    memset(&(cur_arena_p->lock), 0, sizeof(pthread_mutex_t));
    memset(&(cur_arena_p->bin_counts), 0, sizeof(uint16_t) * MAX_BINS);

    return out;
}

/*
 * confirm PAGE_SIZE; initialise main thread arena;
 * bind current arena pointer to current arena key;
 * create the first heap (for malloc's use);
 * refer main_thread_arena_p to the arena initalized;
 */
int initialize_main_arena()
{
    int out = SUCCESS;

    if (malloc_initialized) return out;

    // confirm global variables
    if ((sys_page_size = sysconf(_SC_PAGESIZE)) == -1)
        sys_page_size = HEAP_PAGE_SIZE;

    // ini arena meta data
    if ((out = initialize_arena_meta()) == FAILURE) {
        errno = ENOMEM;
        return out;
    }

    // bind key
    pthread_key_create(&cur_arena_key, thread_destructor);
    pthread_setspecific(cur_arena_key, (void *)cur_arena_p);

    // ini heap meta and block
    if ((out = initialize_new_heap(cur_arena_p)) == FAILURE) {
        errno = ENOMEM;
        return out;
    }

    // raise flag
    malloc_initialized = 1;
    main_thread_arena_p = cur_arena_p;
    return out;
}

/*
 * allocate memory of PAGE_SIZE with sbrk;
 * point the returned addr to the initial block (of MAX_ORDER);
 * call @link_block_to_arena and @link_heap_to_arena
 * to associate block and heap to ar_ptr;
 */
int initialize_new_heap(arena_h_t *ar_ptr)
{
    int out = SUCCESS;
    // ini 12 order block
    block_h_t *block_ptr = NULL;

    if ((block_ptr = (block_h_t *)sbrk(sys_page_size)) == NULL) return FAILURE;

    block_ptr->order = MAX_ORDER;
    block_ptr->order_base_addr = block_ptr;
    block_ptr->status = VACANT;
    block_ptr->next = NULL;
    link_block_to_arena(ar_ptr, (MAX_ORDER - MIN_ORDER), block_ptr);
    // ini heap and link to given pointer
    link_heap_to_arena(ar_ptr, sys_page_size, block_ptr);
    return out;
}

int initialize_thread_arena() { 
    int out = SUCCESS;

    // ini arena meta data
    if ((out = initialize_arena_meta()) == FAILURE) {
        errno = ENOMEM;
        return out;
    }

    // bind key
    pthread_key_create(&cur_arena_key, thread_destructor);
    pthread_setspecific(cur_arena_key, (void *)cur_arena_p);

    // ini heap meta and block
    if ((out = initialize_new_heap(cur_arena_p)) == FAILURE) {
        errno = ENOMEM;
        return out;
    }

    // link arena
    arena_h_t *itr = main_thread_arena_p, *prev_itr = NULL;
    while (itr != NULL) {
        prev_itr = itr;
        itr = itr->next;
    }
    if (prev_itr != NULL) {
        prev_itr->next = cur_arena_p;
    } else {
        main_thread_arena_p = cur_arena_p;
    }
    return out;
}

/*
 * assign hook; check for initialize state for current thread;
 * convert size to order, and call corresponding handler;
 */
void *__lib_malloc(size_t size)
{
    block_h_t *ret_addr = NULL;

    // hook & thread ini
    __hook lib_hook = __malloc_hook;
    if (lib_hook != NULL) {
        return (*lib_hook)(size, __builtin_return_address(0));
    }
    if (cur_arena_p == NULL && initialize_thread_arena()) {
        errno = ENOMEM;
        return NULL;
    }

    // lock and increment stats
    pthread_mutex_lock(&cur_arena_p->lock);
    mallinfo_global.alloreqs++;
    cur_mallinfo.alloreqs++;

    // find required order
    uint8_t size_order = SIZE_TO_ORDER(size + sizeof(block_h_t));
    if (size_order < MIN_ORDER) size_order = MIN_ORDER;

    // allocate using sbkr or mmap
    if (size_order <= MAX_ORDER) {
        if ((ret_addr = find_vacant_block(cur_arena_p,
                                          (size_order - MIN_ORDER))) != NULL ||
            (ret_addr = sbrk_new_block(cur_arena_p, size_order)) != NULL) {
            ret_addr->status = IN_USE;
            ret_addr = (void *)((char *)ret_addr + sizeof(block_h_t));
        }
    } else {
        if ((ret_addr = find_vacant_mmap_block(cur_arena_p, size_order)) !=
                NULL ||
            (ret_addr = mmap_new_block(cur_arena_p, size_order)) != NULL) {
            ret_addr->status = IN_USE;
            ret_addr = (void *)((char *)ret_addr + sizeof(block_h_t));
        }
    }

    // unlock and wrap up
    pthread_mutex_unlock(&cur_arena_p->lock);
    return (void *)ret_addr;
}

void *malloc(size_t size) __attribute__((weak, alias("__lib_malloc")));
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

// ini extern
// FIXME: a better way to organise ?
int no_of_arenas = 1;
int no_of_processors = 1;
long sys_page_size = HEAP_PAGE_SIZE;
bool malloc_initialized = 0;
arena_h_t *main_thread_arena_p;
__thread heap_h_t *cur_base_heap_p;
__thread arena_h_t *cur_arena_p;
__thread pthread_key_t cur_arena_key;
__thread mallinfo cur_mallinfo = (mallinfo){0, 0, 0, 0, 0, 0};
mallinfo mallinfo_global = (mallinfo){0, 0, 0, 0, 0, 0};

void insert_block_to_arena(arena_h_t *ar_ptr, uint8_t bin_index,
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

void insert_heap_to_arena(arena_h_t *ar_ptr, size_t size, block_h_t *block_ptr)
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

void *divide_block_and_add_to_bins(arena_h_t *ar_ptr, block_h_t *mem_block_ptr,
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
    // add first buddy to arena
    insert_block_to_arena(ar_ptr, block_size_order - 1 - MIN_ORDER,
                          mem_block_1);
    return mem_block_2;
}

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
            ret_ptr = divide_block_and_add_to_bins(ar_ptr, block, block->order);
        }
    }

    return ret_ptr;
}

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

block_h_t *sbrk_new_block(arena_h_t *ar_ptr, uint8_t size_order)
{
    if (initialize_new_heap(ar_ptr) == FAILURE) {
        errno = ENOMEM;
        return NULL;
    }
    return find_vacant_block(ar_ptr, size_order - MIN_ORDER);
}

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

    block_ptr = (block_h_t *) mmapped;
    block_ptr->order = size_order;
    block_ptr->order_base_addr = mmapped;
    block_ptr->status = VACANT;
    block_ptr->next = NULL;
    insert_block_to_arena(ar_ptr, (MAX_ORDER - MIN_ORDER), block_ptr);

    // ini heap and link to given pointer
    insert_heap_to_arena(ar_ptr, size, block_ptr);
    return block_ptr;
}

void *initialize_malloc_lib(size_t size, const void *caller)
{
    // TODO: fork handles
    // if (pthread_atfork(pthread_atfork_prepare, pthread_atfork_parent,
    // 		pthread_atfork_child))
    // 	return NULL;

    if (initialize_main_arena()) {
        return NULL;
    }

    __malloc_hook = NULL;
    return malloc(size);
}

void thread_destructor(void *ptr) { return; }

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

int initialize_main_arena()
{
    int out = SUCCESS;

    if (malloc_initialized) return out;

    // confirm global variables
    if ((sys_page_size = sysconf(_SC_PAGESIZE)) == -1)
        sys_page_size = HEAP_PAGE_SIZE;
    if ((no_of_processors = sysconf(_SC_NPROCESSORS_ONLN)) == -1)
        no_of_processors = 1;

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
    insert_block_to_arena(ar_ptr, (MAX_ORDER - MIN_ORDER), block_ptr);

    // ini heap and link to given pointer
    insert_heap_to_arena(ar_ptr, sys_page_size, block_ptr);
    return out;
}

int initialize_thread_arena() { return SUCCESS; }

void *__lib_malloc(size_t size)
{
    block_h_t *ret_addr = NULL;

    __hook lib_hook = __malloc_hook;

    if (lib_hook != NULL) {
        return (*lib_hook)(size, __builtin_return_address(0));
    }

    if (cur_arena_p == NULL && initialize_thread_arena()) {
        errno = ENOMEM;
        return NULL;
    }
    pthread_mutex_lock(&cur_arena_p->lock);
    mallinfo_global.alloreqs++;
    cur_mallinfo.alloreqs++;

    uint8_t size_order = SIZE_TO_ORDER(size + sizeof(block_h_t));
    if (size_order < MIN_ORDER) size_order = MIN_ORDER;

    if (size_order <= MAX_ORDER) {
        if ((ret_addr = find_vacant_block(cur_arena_p,
                                          (size_order - MIN_ORDER))) != NULL ||
            (ret_addr = sbrk_new_block(cur_arena_p, size_order)) != NULL) {
            ret_addr->status = IN_USE;
            ret_addr = (void *)(ret_addr + sizeof(block_h_t));
        }
    } else {
        if ((ret_addr = find_vacant_mmap_block(cur_arena_p, size_order)) != NULL ||
                (ret_addr = mmap_new_block(cur_arena_p, size_order)) != NULL) {
            ret_addr->status = IN_USE;
            ret_addr = (void *)(ret_addr + sizeof(block_h_t));
        }
    }

    pthread_mutex_unlock(&cur_arena_p->lock);
    return (void *)ret_addr;
}

void *malloc(size_t size) __attribute__((weak, alias("__lib_malloc")));
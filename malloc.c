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

block_header_t* find_free_block(arena_header_t* ar_ptr, uint8_t bin_index)
{
    if (bin_index == (MAX_BINS - 1)) return NULL;

    block_header_t* ret_ptr = NULL;

    if (ar_ptr->bins[bin_index] != NULL) {
        ret_ptr = ar_ptr->bins[bin_index];
        ar_ptr->bins[bin_index] = ret_ptr->block_data.next_block_ptr;
        ar_ptr->bin_counts[bin_index]--;
    } else {
        block_header_t* block =
            (block_header_t*)find_free_block(ar_ptr, bin_index + 1);

        if (block != NULL) {
            ret_ptr = divide_blocks_and_add_to_bins(ar_ptr, bin_index, block,
                                                    block->block_size);
        }
    }

    return ret_ptr;
}

block_header_t* find_free_mmap_block(arena_header_t* ar_ptr, uint8_t size_order)
{
    block_header_t *free_list_itr = ar_ptr->bins[MMAP_BIN], *ret_ptr = NULL,
                   *prev_block = NULL;

    while (free_list_itr != NULL && free_list_itr->block_size < size_order) {
        prev_block = free_list_itr;
        free_list_itr = free_list_itr->block_data.next_block_ptr;
    }

    if (free_list_itr != NULL && prev_block == NULL) {
        ret_ptr = free_list_itr;
        ar_ptr->bins[MMAP_BIN] = ret_ptr->block_data.next_block_ptr;
        ar_ptr->bin_counts[MMAP_BIN]--;
    } else if (free_list_itr != NULL && prev_block != NULL) {
        ret_ptr = free_list_itr;
        prev_block->block_data.next_block_ptr =
            ret_ptr->block_data.next_block_ptr;
        ar_ptr->bin_counts[MMAP_BIN]--;
    }

    return ret_ptr;
}

void link_to_existing_heap(arena_header_t* ar_ptr, heap_header_t* heap_ptr)
{
    heap_header_t* main_heap_ptr = (heap_header_t*)ARENA_TO_HEAP_PTR(ar_ptr);
    heap_ptr->next_heap_ptr = main_heap_ptr->next_heap_ptr;
    main_heap_ptr->next_heap_ptr = heap_ptr;
    ar_ptr->no_of_heaps++;
}

void* allocate_new_block(arena_header_t* ar_ptr, size_t size_order)
{
    block_header_t* new_block = NULL;

    heap_header_t* heap_ptr = NULL;
    size_t heap_size = sys_page_size + sizeof(heap_header_t);

    if ((heap_ptr = (heap_header_t*)sbrk(heap_size)) == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    heap_ptr->arena_ptr = ar_ptr;
    heap_ptr->heap_mmaped = NOT_MMAPED;
    heap_ptr->heap_size = heap_size;
    heap_ptr->next_heap_ptr = NULL;
    heap_ptr->heap_base_addr = heap_ptr;

    link_to_existing_heap(ar_ptr, heap_ptr);

    new_block = (block_header_t*)HEAP_TO_BLOCK_PTR(heap_ptr);
    new_block->allocated = NOT_ALLOCATED;
    new_block->block_size = MAX_HEAP_BLOCK_SIZE_ORDER;
    new_block->block_data.next_block_ptr = NULL;
    new_block->block_base_address = new_block;

    sort_and_add(ar_ptr, ORDER_TO_BIN_INDEX(MAX_HEAP_BLOCK_SIZE_ORDER),
                 new_block);

    return find_free_block(ar_ptr, size_order - MIN_HEAP_BLOCK_SIZE_ORDER);
}

void* mmap_and_allocate(arena_header_t* ar_ptr, size_t size)
{
    int heap_size = pow(2, SIZE_TO_ORDER(size)) + sizeof(heap_header_t);

    block_header_t* block_ptr = NULL;
    heap_header_t* heap_ptr = NULL;

    if ((heap_ptr = (heap_header_t*)mmap(
             NULL, heap_size, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0)) == MAP_FAILED) {
        errno = ENOMEM;
        return NULL;
    }

    heap_ptr->arena_ptr = ar_ptr;
    heap_ptr->heap_mmaped = MMAPED;
    heap_ptr->heap_size = heap_size;
    heap_ptr->next_heap_ptr = NULL;
    heap_ptr->heap_base_addr = heap_ptr;

    link_to_existing_heap(ar_ptr, heap_ptr);

    block_ptr = (block_header_t*)HEAP_TO_BLOCK_PTR(heap_ptr);
    block_ptr->mmaped = MMAPED;
    block_ptr->block_size = SIZE_TO_ORDER(size);
    block_ptr->block_base_address = block_ptr;
    block_ptr->block_data.next_block_ptr = NULL;

    return block_ptr;
}

void* malloc(size_t size)
{
    block_header_t* ret_addr = NULL;

    __hook lib_hook = __malloc_hook;

    if (lib_hook != NULL) {
        return (*lib_hook)(size, __builtin_return_address(0));
    }

    if (ts_arena_ptr == NULL && initialize_thread_arena()) {
        errno = ENOMEM;
        return NULL;
    }

    pthread_mutex_lock(&ts_arena_ptr->arena_lock);
    ts_arena_ptr->total_alloc_req++;

    uint8_t size_order = SIZE_TO_ORDER(size + DATA_OFFSET);
    if (size_order < MIN_HEAP_BLOCK_SIZE_ORDER)
        size_order = MIN_HEAP_BLOCK_SIZE_ORDER;

    if (size_order <= MAX_HEAP_BLOCK_SIZE_ORDER) {
        if ((ret_addr = find_free_block(
                 ts_arena_ptr, (size_order - MIN_HEAP_BLOCK_SIZE_ORDER))) !=
            NULL) {
            ret_addr->allocated = ALLOCATED;
            ret_addr = (void*)&ret_addr->block_data.data;
        } else if ((ret_addr = allocate_new_block(ts_arena_ptr, size_order)) !=
                   NULL) {
            ret_addr->allocated = ALLOCATED;
            ret_addr = (void*)&ret_addr->block_data.data;
        }
    } else {
        if ((ret_addr = find_free_mmap_block(ts_arena_ptr, size_order)) !=
            NULL) {
            ret_addr->allocated = ALLOCATED;
            ret_addr = (void*)&ret_addr->block_data.data;
        } else if ((ret_addr = mmap_and_allocate(
                        ts_arena_ptr, (size + DATA_OFFSET))) != NULL) {
            ret_addr->allocated = ALLOCATED;
            ret_addr = (void*)&ret_addr->block_data.data;
        }
    }

    pthread_mutex_unlock(&ts_arena_ptr->arena_lock);

    return (void*)ret_addr;
}
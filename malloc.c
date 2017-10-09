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

void* initialize_malloc_lib(size_t size, const void* caller) {
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

void thread_destructor(void* ptr) {
    return;
}

int initialize_main_arena() {
    int out = SUCCESS;
    
    if (malloc_initialized)
        return out;

    if ((sys_page_size = sysconf(_SC_PAGESIZE)) == -1)
        sys_page_size = HEAP_PAGE_SIZE;

    if ((no_of_processors = sysconf(_SC_NPROCESSORS_ONLN)) == -1)
        no_of_processors = 1;

    // ini arena and lin to main thread meta
    cur_arena_p = &main_thread_metadata.arena_ptr;
    cur_arena_p->status = IN_USE;
    cur_arena_p->no_of_heaps = 1;
    cur_arena_p->next = NULL;
    cur_arena_p->total_alloc_req = 0;
    cur_arena_p->total_free_req = 0;
    memset(&(cur_arena_p->lock), 0, sizeof(pthread_mutex_t));
    memset(&(cur_arena_p->bin_counts), 0, sizeof(uint16_t) * MAX_BINS);
    
    // bind key
    pthread_key_create(&cur_arena_key, thread_destructor);
    pthread_setspecific(cur_arena_key, (void *) cur_arena_p);

    // ini 12 order block
    block_h_t *block_ptr = NULL;
    if ((block_ptr = (block_h_t*) sbrk(sys_page_size)) == NULL) {
        errno = ENOMEM;
        out = FAILURE;
        return out;
    }

    block_ptr->order = MAX_ORDER;
    block_ptr->next = NULL;
    block_ptr->order_base_addr = block_ptr;
    block_ptr->status = VACANT;
    insert_block_to_arena(cur_arena_p, (MAX_ORDER - MIN_ORDER), block_ptr);

    // ini first heap and link to main thread meta
    heap_h_t* heap_ptr = &main_thread_metadata.heap_ptr;
	heap_ptr->arena_ptr = cur_arena_p;
	heap_ptr->size = sys_page_size;
	heap_ptr->next = NULL;
	heap_ptr->base_block = block_ptr;

    malloc_initialized = 1;
    return out;
}

int initialize_thread_arena() {
    return SUCCESS;
}

void insert_block_to_arena(arena_h_t *ar_ptr, uint8_t bin_index,
    block_h_t *block_to_insert) {

    ar_ptr->bin_counts[bin_index]++;
    block_to_insert->next = NULL;

    if (ar_ptr->bins[bin_index] == NULL) {  // first comer
        ar_ptr->bins[bin_index] = block_to_insert;
    } else if (block_to_insert < ar_ptr->bins[bin_index]) { // link as start tail
        block_to_insert->next = ar_ptr->bins[bin_index];
        ar_ptr->bins[bin_index] = block_to_insert;
    } else {  // link through all existing bins of the same order
        block_h_t* itr = ar_ptr->bins[bin_index], *prev_block = NULL;

        while (itr != NULL && block_to_insert > itr) {
            prev_block = itr;
            itr = itr->next;
        }

        if (itr == NULL) {  // link as end tail
            block_to_insert->next = NULL;
            prev_block->next = block_to_insert;
        } else { // link in the middle
            block_to_insert->next =
                    prev_block->next;
            prev_block->next = block_to_insert;
        }
    }
}

block_h_t *find_free_block(arena_h_t* ar_ptr, uint8_t bin_index) {
    // last bin is for size>4096
    if (bin_index == (MAX_BINS - 1)) {
        printf("something is wrong\n");
        return NULL;
    }

    block_h_t *ret_ptr = NULL;

    if (ar_ptr->bins[bin_index] != NULL) {
        ret_ptr = ar_ptr->bins[bin_index];
        ar_ptr->bins[bin_index] = ret_ptr->next;
        ar_ptr->bin_counts[bin_index]--;
    } else {
        block_h_t* block = (block_h_t*) find_free_block(ar_ptr,
                bin_index + 1);

        if (block != NULL) {
            ret_ptr = divide_block_and_add_to_bins(ar_ptr, bin_index, block,
                    block->order);
        }
    }

    return ret_ptr;
}

void *divide_block_and_add_to_bins(arena_h_t *ar_ptr, uint8_t bin_index,
    block_h_t *mem_block_ptr, int block_size_order) {
    int size = pow(2, block_size_order);
    void *mem_block_1 = mem_block_ptr;
    void *mem_block_2 = (((char*) mem_block_ptr) + (size / 2));

    block_h_t *block_hdr = NULL;
    // buddy 1
    block_hdr = (block_h_t*) mem_block_1;
    block_hdr->status = VACANT;
    block_hdr->order = block_size_order - 1;
    block_hdr->order_base_addr = mem_block_ptr->order_base_addr;
    // buddy 2
    block_hdr = (block_h_t*) mem_block_2;
    block_hdr->status = VACANT;
    block_hdr->order = block_size_order - 1;
    block_hdr->order_base_addr = mem_block_ptr->order_base_addr;
    // add first buddy to arena
    insert_block_to_arena(ar_ptr, bin_index, mem_block_1);
    return mem_block_2;

}

void* __lib_malloc(size_t size)
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
    cur_arena_p->total_alloc_req++;

    uint8_t size_order = SIZE_TO_ORDER(size + sizeof(block_h_t));
    if (size_order < MIN_ORDER)
        size_order = MIN_ORDER;

    if (size_order <= MAX_ORDER) {
        if ((ret_addr = find_free_block(cur_arena_p,
                (size_order - MIN_ORDER))) != NULL) {
            ret_addr = (void*) (ret_addr + sizeof(block_h_t));
            printf("%x\n", ret_addr);
        }
        // else if ((ret_addr = allocate_new_block(cur_arena_p, size_order))
        //         != NULL) {
        //     ret_addr = (void*) &ret_addr->block_data.data;
        // }
    } else {
        // TODO: handle larger than 4096 blocks
        errno = ENOMEM;
        return NULL;
    }

    pthread_mutex_unlock(&cur_arena_p->lock);
    return (void*) ret_addr;
}

void *malloc(size_t size) __attribute__((weak, alias ("__lib_malloc")));
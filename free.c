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

int validate_addr(arena_h_t *ar_ptr, void *mem_ptr)
{
    int ret_val = VALID;
    if (mem_ptr == NULL)
        ret_val = INVALID;
    else {
        heap_h_t *heap_ptr = (heap_h_t *)ar_ptr->base_heap;
        ret_val = INVALID;
        while (heap_ptr) {
            if ((mem_ptr >= (void *)heap_ptr->base_block) &&
                (mem_ptr <=
                 (void *)(((char *)heap_ptr->base_block) + heap_ptr->size))) {
                ret_val = VALID;
                break;
            }
            heap_ptr = heap_ptr->next;
        }
    }
    return ret_val;
}


block_h_t *find_vacant_buddy(block_h_t *block_ptr)
{
    block_h_t *buddy_ptr = NULL;
    size_t size = pow(2, block_ptr->order);

    if (block_ptr->order_base_addr < (void*) block_ptr)
        buddy_ptr = (block_h_t *)((char *)block_ptr - size);
    else
        buddy_ptr = (block_h_t *)((char *)block_ptr + size);

    if (buddy_ptr->status == VACANT) return buddy_ptr;
    return NULL;
}

void release_buddy_block(arena_h_t *ar_ptr, block_h_t *block_ptr)
{
    uint8_t bin_index;
    block_h_t *itr, *prev_itr, *merged_block_ptr, *buddy_block_ptr = NULL;
    uint8_t order = block_ptr->order;

    // stop releasing when reaching max or IN_USE buddy
    if (order == MAX_ORDER ||
        (buddy_block_ptr = find_vacant_buddy(block_ptr)) == NULL) {
        insert_block_to_arena(ar_ptr, order - MIN_ORDER, block_ptr);
        return;
    }

    // find reference
    bin_index = order - MIN_ORDER;
    itr = ar_ptr->bins[bin_index];

    while(itr == NULL && itr != buddy_block_ptr) {
        prev_itr = itr;
        itr = itr->next;
    }

    // no reference to buddy, insert block and return
    if (prev_itr == NULL || itr != buddy_block_ptr) {
        insert_block_to_arena(ar_ptr, order - MIN_ORDER, block_ptr);
        return;
    }

    // dereference
    if (itr != NULL && prev_itr != NULL)
        prev_itr->next = block_ptr->next;
    else if (itr == buddy_block_ptr)
        ar_ptr->bins[bin_index] = itr->next;
    ar_ptr->bin_counts[bin_index]--;

    // merge
    merged_block_ptr = (block_h_t *) block_ptr->order_base_addr;
    merged_block_ptr->order = order + 1;
    release_buddy_block(ar_ptr, merged_block_ptr);
}

void __lib_free(void *mem_ptr)
{
    block_h_t *block_ptr = NULL;

    if (cur_arena_p == NULL) {
        return;
    }

    if (validate_addr(cur_arena_p, mem_ptr) == INVALID) return;

    block_ptr = (block_h_t *)mem_ptr - sizeof(block_h_t);

    pthread_mutex_lock(&cur_arena_p->lock);
    cur_arena_p->total_free_req++;

    if (block_ptr->order <= MAX_ORDER) {
        release_block_and_buddy(cur_arena_p, block_ptr);
    } else {
        // unmap
    }

    pthread_mutex_unlock(&cur_arena_p->lock);
}
void *free(void *mem_ptr) __attribute__((weak, alias("__lib_free")));
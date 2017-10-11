#include <stdio.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <stddef.h>
#include "common.h"

void* __lib_realloc(void* ptr, size_t size)
{
    // If ptr is NULL, then  the  call  is  equivalent  to  mal-
    // loc(size), for all values of size;
    // if size is equal to zero, and ptr is not NULL, then the
    // call is equivalent to free(ptr).
    if (ptr == NULL)
        return __lib_malloc(size);
    else if (size == 0) {
        __lib_free(ptr);
        return NULL;
    }

    void *ret_ptr = __lib_malloc(size);
    pthread_mutex_lock(&cur_arena_p->lock);

    // find minimum range, copy the content from old to new
    block_h_t *new_ptr = (block_h_t *) ((char *)ret_ptr - sizeof(block_h_t));
    block_h_t *old_ptr = (block_h_t *) ((char *)ptr - sizeof(block_h_t));
    uint8_t order = (new_ptr->order < old_ptr->order) ? new_ptr->order : old_ptr->order;
    size_t memcpy_size = pow(2, order) - sizeof(block_h_t);
    memcpy(ret_ptr, ptr, memcpy_size);

    // unlock, and release old
    pthread_mutex_unlock(&cur_arena_p->lock);
    __lib_free(ptr);
    return ret_ptr;
}
void *realloc(void* ptr, size_t size) __attribute__((weak, alias("__lib_realloc")));
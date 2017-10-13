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

__realloc_hook_t __realloc_hook = (__realloc_hook_t)initialize_realloc;

void *initialize_realloc(void *ptr, size_t size, const void *caller)
{
    if (initialize_main_arena()) {
        return NULL;
    }

    __realloc_hook = NULL;
    return realloc(ptr, size);
}

/*
 * allocate a memory region of size $size;
 * copy the content pointed to by $ptr;
 * leave what's beyond min(size, sizeof(mem_pointed_to_by_ptr)) uninitialized;
 * finally free the old mem_pointed_to_by_ptr;
 */
void *__lib_realloc(void *ptr, size_t size)
{
    __realloc_hook_t lib_hook = __realloc_hook;
    if (lib_hook != NULL) {
        return (*lib_hook)(ptr, size, __builtin_return_address(0));
    }

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
    block_h_t *new_ptr = (block_h_t *)((char *)ret_ptr - sizeof(block_h_t));
    block_h_t *old_ptr = (block_h_t *)((char *)ptr - sizeof(block_h_t));
    uint8_t order =
        (new_ptr->order < old_ptr->order) ? new_ptr->order : old_ptr->order;
    size_t memcpy_size = pow(2, order) - sizeof(block_h_t);
    memcpy(ret_ptr, ptr, memcpy_size);

    // unlock, and release old
    pthread_mutex_unlock(&cur_arena_p->lock);
    __lib_free(ptr);
    return ret_ptr;
}
void *realloc(void *ptr, size_t size)
    __attribute__((weak, alias("__lib_realloc")));
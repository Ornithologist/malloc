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

void* realloc(void* ptr, size_t size)
{
    void* ret_ptr = NULL;

    if (size != 0)
        ret_ptr = __lib_malloc(size);
    else if (ptr != NULL && size == 0) {
        __lib_free(ptr);
        return NULL;
    }

    if (ptr == NULL || !is_valid_address(ts_arena_ptr, ptr)) return ret_ptr;

    pthread_mutex_lock(&ts_arena_ptr->arena_lock);

    block_header_t *block_ptr = NULL,
                   *prev_block_ptr = (block_header_t*)BLOCK_HEADER_PTR(ptr);

    block_ptr = (block_header_t*)BLOCK_HEADER_PTR(ret_ptr);

    size_t copy_size = (prev_block_ptr->block_size > block_ptr->block_size)
                           ? block_ptr->block_size
                           : prev_block_ptr->block_size;

    copy_size = pow(2, copy_size) - DATA_OFFSET;

    memset(&block_ptr->block_data.data, 0, copy_size);
    memcpy(&block_ptr->block_data.data, &prev_block_ptr->block_data.data,
           copy_size);

    pthread_mutex_unlock(&ts_arena_ptr->arena_lock);

    __lib_free(ptr);

    return ret_ptr;
}
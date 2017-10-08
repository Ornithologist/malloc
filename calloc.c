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

void* calloc(size_t nmemb, size_t size)
{
    block_header_t* block_ptr = NULL;
    void* ret_ptr = __lib_malloc(nmemb * size);

    if (ret_ptr == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    block_ptr = (block_header_t*)BLOCK_HEADER_PTR(ret_ptr);
    int block_size = (pow(2, block_ptr->block_size) - DATA_OFFSET);
    memset(&block_ptr->block_data.data, 0, block_size);

    return ret_ptr;
}
// void *calloc(size_t nmemb, size_t size) __attribute__((weak, alias("__lib_calloc")));
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

void *__lib_calloc(size_t nmemb, size_t size)
{
    if (nmemb == 0 || size == 0)
        return NULL;

    void* ret_ptr = __lib_malloc(nmemb * size);
    if (ret_ptr == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    block_h_t *block_ptr = (block_h_t *) (ret_ptr - sizeof(block_h_t));
    size_t memset_size = pow(2, block_ptr->order) - sizeof(block_h_t);
    memset(ret_ptr, 0, memset_size);
    return ret_ptr;
}
void *calloc(size_t nmemb, size_t size) __attribute__((weak, alias("__lib_calloc")));
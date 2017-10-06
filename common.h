#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#ifndef __COMMON_H__
#define __COMMON_H__

#define MIN_NUM_ARENA 2
#define BASE_BLOCK_BYTES 32          // 32 B
#define MMAP_THRESHOLD_BYTES 128000  // 128 kB
#define SIZE_TO_ORDER(size) (ceil(log(ceil(size / BASE_BLOCK_BYTES)) / log(2)))
#define BASE_ORDER SIZE_TO_ORDER(BASE_BLOCK_BYTES)
#define MAX_ORDER SIZE_TO_ORDER(MMAP_THRESHOLD_BYTES)

typedef struct _block_meta {
    void *addr;
    uint8_t order;
    uint8_t allocated;
    uint8_t mmaped;
    union {
        struct _block_meta *next;
        void *data[0];
    } block_data;
} block_meta_t;

typedef struct _arena_meta {
    pthread_mutex_t lock;
    block_meta_t *blocks[] unit16_t heap_count;
    unit8_t thread_count;
    size_t alloc_count;
    size_t free_count;
    size_t block_count;
    block_meta_t *blocks[MAX_ORDER + 2];  // normal blocks plus one big block
    int in_use;
    struct _arena_meta *next;
} arena_meta_t;

typedef struct _heap_meta {
    size_t size;
    unit8_t mmapped;
    arena_meta_t *arena_p;
    struct _heap_meta *next;
    void *content;
} heap_meta_t;

typedef struct _thread_meta {
    arena_meta_t *arena_p;
    heap_meta_t *heap_p;
} thread_meta_t;

block_meta_t *alloc_by_order(arena_meta_t *arena_p, uint8_t order);

#endif
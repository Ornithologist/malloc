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

void free(void* mem_ptr) {
	block_header_t* block_ptr = NULL;

	if (ts_arena_ptr == NULL) {
		return;
	}

	if (!is_valid_address(ts_arena_ptr, mem_ptr))
		return;

	block_ptr = (block_header_t*) BLOCK_HEADER_PTR( mem_ptr );

	pthread_mutex_lock(&ts_arena_ptr->arena_lock);
	ts_arena_ptr->total_free_req++;

	release_block_to_bin(ts_arena_ptr, block_ptr);

	pthread_mutex_unlock(&ts_arena_ptr->arena_lock);
}
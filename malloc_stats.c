#include <assert.h>
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

void __lib_malloc_stats(void)
{
    pthread_mutex_lock(&cur_arena_p->lock);
    arena_h_t *ar_itr = (arena_h_t *)main_thread_arena_p;
    int narenas = 0;
    int ar_idx = 0;
    long arena = 0;

    while (ar_itr != NULL) {
        narenas += 1;
        mallinfo_t *cur_mal_p =
            (mallinfo_t *)((char *)ar_itr + sizeof(arena_h_t));
        arena += cur_mal_p->arena;
        int freeblks = cur_mal_p->freeblks;
        int alloblks = cur_mallinfo_p->alloblks;
        int tblks = freeblks + alloblks;
        int alloreqs = cur_mallinfo_p->alloreqs;
        int freereqs = cur_mallinfo_p->freereqs;

        printf(
            "============================Arena Info "
            "%d=============================\n",
            ar_idx++);
        printf("\t Total number of blocks: %d\n", tblks);
        printf("\t Used blocks: %d\n", alloblks);
        printf("\t Free blocks: %d\n", freeblks);
        printf("\t Total allocation requests: %d\n", alloreqs);
        printf("\t Total free requests: %d\n", freereqs);

        ar_itr = ar_itr->next;
    }

    if (narenas > 0) {
        printf(
            "============================Summary "
            "Info=============================\n");
        printf("\t Total Size of Arenas: %ld B\n", arena);
        printf("\t Total Number of Arenas: %d\n", narenas);
    }

    pthread_mutex_unlock(&cur_arena_p->lock);
    return;
}
void malloc_stats() __attribute__((weak, alias("__lib_malloc_stats")));
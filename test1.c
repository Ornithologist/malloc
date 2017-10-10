#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void *allocate(size_t size) {
    void *mem = malloc(size);
    assert(mem != NULL);
    printf("Successfully malloc'd %zu bytes at addr %p\n", size, mem);
    return mem;
}

void deallocate(void *mem_ptr) {
    free(mem_ptr);
    printf("Successfully free'd from addr %p\n", mem_ptr);
}

int main(int argc, char **argv)
{
    size_t size1 = 12;
    // size_t size2 = 1100;
    size_t size3 = 2200;

    
    // void *mem2 = allocate(size2);
    void *mem1 = allocate(size1);
    void *mem3 = allocate(size3);
    // deallocate(mem2);
    deallocate(mem1);
    deallocate(mem3);

    return 0;
}
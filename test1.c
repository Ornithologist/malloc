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

void *callocate(size_t a, size_t b) {
    void *mem = calloc(a, b);
    assert(mem != NULL);
    int *test = (int *) mem;
    if ((*test) == 0)
        printf("Successfully calloc'd %zu bytes at addr %p\n", a * b, mem);
    else
        printf("calloc test %d\n", test);
    return mem;
}

void *reallocate(void *ptr, size_t size) {
    void *mem = realloc(ptr, size);
    assert(mem != NULL);
    printf("Successfully realloc'd at addr %p\n", mem);
}

int main(int argc, char **argv)
{
    size_t size1 = 12;
    size_t size2 = 1100;
    size_t size3 = 2200;
    
    void *mem1 = allocate(size1);
    void *mem2 = allocate(size2);
    void *mem3 = allocate(size3);

    reallocate(mem3, 1200);
    
    deallocate(mem2);
    deallocate(mem1);
    // deallocate(mem3);

    // mem1 = allocate(450);
    // mem2 = allocate(150);
    // mem3 = allocate(23);

    // deallocate(mem3);
    // deallocate(mem2);
    // deallocate(mem1);

    // mem1 = allocate(8900);
    // mem2 = allocate(5600);
    // mem3 = allocate(2);
    // deallocate(mem3);
    // deallocate(mem1);
    // deallocate(mem2);

    // void *mem = malloc(2400);
    int *a = (int *) malloc(sizeof(int));
    (*a) = 18;
    deallocate(a);
    void *mem = callocate(45, 50);
    deallocate(mem);

    return 0;
}
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    size_t size = 12;
    void *mem = malloc(size);
    printf("Successfully malloc'd %zu bytes at addr %p\n", size, mem);
    assert(mem != NULL);

    free(mem);
    printf("Successfully free'd %zu bytes from addr %p\n", size, mem);

    size = 35;
    mem = malloc(size);
    printf("Successfully malloc'd %zu bytes at addr %p\n", size, mem);
    assert(mem != NULL);

    free(mem);
    printf("Successfully free'd %zu bytes from addr %p\n", size, mem);

    size = 66;
    mem = malloc(size);
    printf("Successfully malloc'd %zu bytes at addr %p\n", size, mem);
    assert(mem != NULL);

    free(mem);
    printf("Successfully free'd %zu bytes from addr %p\n", size, mem);

    size = 130;
    mem = malloc(size);
    printf("Successfully malloc'd %zu bytes at addr %p\n", size, mem);
    assert(mem != NULL);

    free(mem);
    printf("Successfully free'd %zu bytes from addr %p\n", size, mem);

    size = 270;
    mem = malloc(size);
    printf("Successfully malloc'd %zu bytes at addr %p\n", size, mem);
    assert(mem != NULL);

    free(mem);
    printf("Successfully free'd %zu bytes from addr %p\n", size, mem);

    size = 550;
    mem = malloc(size);
    printf("Successfully malloc'd %zu bytes at addr %p\n", size, mem);
    assert(mem != NULL);

    free(mem);
    printf("Successfully free'd %zu bytes from addr %p\n", size, mem);

    size = 1100;
    mem = malloc(size);
    printf("Successfully malloc'd %zu bytes at addr %p\n", size, mem);
    assert(mem != NULL);

    free(mem);
    printf("Successfully free'd %zu bytes from addr %p\n", size, mem);

    size = 2200;
    mem = malloc(size);
    printf("Successfully malloc'd %zu bytes at addr %p\n", size, mem);
    assert(mem != NULL);

    free(mem);
    printf("Successfully free'd %zu bytes from addr %p\n", size, mem);
    return 0;
}
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct MallocHeader
{
  size_t size;
} MallocHeader;

void *realloc(void *ptr, size_t size)
{
  // Allocate new memory (if needed) and copy the bits from old location to new.

  return NULL;
}
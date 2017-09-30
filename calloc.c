#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "common.h"

typedef struct MallocHeader
{
  size_t size;
} MallocHeader;

void *calloc(size_t nmemb, size_t size)
{
  return NULL;
}
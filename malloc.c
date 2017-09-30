#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "common.h"

void *malloc(size_t size)
{
  // TODO: Validate size.
  size_t allocSize = size + sizeof(MallocHeader);

  void *ret = mmap(0, allocSize, PROT_READ | PROT_WRITE,
                   MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  assert(ret != MAP_FAILED);

  // We can't use printf here because printf internally calls `malloc` and thus
  // we'll get into an infinite recursion leading to a segfault.
  // Instead, we first write the message into a string and then use the `write`
  // system call to display it on the console.
  char buf[1024];
  snprintf(buf, 1024, "%s:%d malloc(%zu): Allocated %zu bytes at %p\n",
           __FILE__, __LINE__, size, allocSize, ret);
  write(STDOUT_FILENO, buf, strlen(buf) + 1);

  MallocHeader *hdr = (MallocHeader*) ret;
  hdr->size = allocSize;

  return ret + sizeof(MallocHeader);
}
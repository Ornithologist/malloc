#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include "common.h"

void free(void *ptr)
{
  MallocHeader *hdr = ptr - sizeof(MallocHeader);
  // We can't use printf here because printf internally calls `malloc` and thus
  // we'll get into an infinite recursion leading to a segfault.
  // Instead, we first write the message into a string and then use the `write`
  // system call to display it on the console.
  char buf[1024];
  snprintf(buf, 1024, "%s:%d free(%p): Freeing %zu bytes from %p\n",
           __FILE__, __LINE__, ptr, hdr->size, hdr);
  write(STDOUT_FILENO, buf, strlen(buf) + 1);
  munmap(hdr, hdr->size);
}
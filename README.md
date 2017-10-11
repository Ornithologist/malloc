# malloc

A malloc library that provides the dynamic memory allocation routines _malloc_, _free_, _calloc_, _realloc_ and other helper functions.

## Design Overview

Upon each _malloc_ request, it searches through `block`s, the most granular data structure in the libaray that represents a memory region to be used by _malloc_, _calloc_, or _realloc_.

Buddy allocation is used for memory requests with size smaller than 4096 bytes. The smallest `block`  is 32 bytes. There's no upper limit. `block` keeps a reference to its sibling of the same size, thus forming a linked list.

When no free `block` is available, _malloc_ allocates new memory by _sbrk()_ or _mmap()_, depending on the requested size.

Each time _sbrk()_ or _mmap()_ is called, a meta data structure `heap` is created. `heap` maintains the starting address of the newly allocated memory region, and the size of the it. `heap` also keeps a reference to its next sibling from the same thread to form a linked list.

A thread memory `arena` is maintained to manage the `block` and `heap` linked lists.

## Design Decisions


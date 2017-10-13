# malloc

A malloc library that provides the dynamic memory allocation routines _malloc_, _free_, _calloc_, _realloc_ and other helper functions.

## Build and Test

To build, run ```make lib```.

You might want to run ```make clean``` before building.

To test, run ```make check```.

## Design Overview

Upon each _malloc_ request, it searches through `block`s, the most granular data structure in the libaray that represents a memory region to be used by _malloc_, _calloc_, or _realloc_.

Buddy allocation is used for memory requests with size smaller than 4096 bytes. The smallest `block`  is 32 bytes. There's no upper limit. `block` keeps a reference to its sibling of the same size, thus forming a linked list.

When no free `block` is available, _malloc_ allocates new memory by _sbrk()_ or _mmap()_, depending on the requested size.

Each time _sbrk()_ or _mmap()_ is called, a meta data structure `heap` is created. `heap` maintains the starting address of the newly allocated memory region, and the size of the it. `heap` also keeps a reference to its next sibling from the same thread to form a linked list.

A thread memory `arena` is maintained to manage the `block` and `heap` linked lists.

## Design Decisions

* The biggest memory request that is going to be handled by the buddy system is of size smaller than 4096 bytes minus the size of the block header structure. The decision was made to reduce the number of times a block gets divided into buddies. Further, the buddy system tends to be much less efficient in terms of memory usage when handling large size requests. For example, a memory request of 2100 bytes would receive a block of  4096 bytes, since the previous order of blocks(2048 bytes) are not big enough to fulfill the request.

* A list of linked block lists is maintained in the `arena` instance to keep track of the free blocks. Each linked list represents a number of free blocks in different order. All the blocks in one linked list are of the same order, and are pre-sorted based on their starting address. This enables faster search as compared to one-dimensional linked lists. In the latter case, the library would have to search through blocks of different orders to retrieve one free block that fits the request.

* A `heap` instance represents the memory allocated each time _malloc_ library requests kernel using _sbrk()_ or _mmap()_. During the initial design phase, I thought this is necessary so that the arena can keep track of each memory allocation requests it made to the kernal, thus made it easier to calculate the total size of arena. In the later phase of development I realised that it is in fact unnecessary. Due to time constraint I chose not to refactor the code under the risk of breaking it. However, it is necessary to clean-up this portion of design in order to further improve it. 

* I decided to store all `arena` and `heap` instances in a memory region allocated using _sbrk()_ upon the initiation of a new thread. This is a bad design that results into a limited number of `heap`s creatable per thread -- (PAGE_SIZE - sizeof(arena)) / sizeof(heap). However, it is necessary for the library to work. During the development I encountered a memory corruption bug that, were I initiate the `arena` and `heap` instances  like
```
heap_h_t new_heap = {0}
```
so that they should go to stack, they would in fact enter a memory region that is above stack and suffer corruption afterwards. This "bad" design decision was made along with the previous one. To solve it, we need 1) solve the memory corruption bug; 2) remove the `heap` structure completely.

## Known bugs and errors
* As above mentioend, the biggest limitation of the library is the number of _sbrk()_ or _mmap()_ requests it can make (around 160 times).
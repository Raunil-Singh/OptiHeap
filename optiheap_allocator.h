#ifndef OPTIHEAP
#define OPTIHEAP

#include <stddef.h>

void optiheap_allocator_init(void);
void* optiheap_allocate(size_t size);
void* optiheap_free(void* ptr);

#endif
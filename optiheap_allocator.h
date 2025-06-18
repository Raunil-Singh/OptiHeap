#ifndef OPTIHEAP
#define OPTIHEAP

#include <stddef.h>

void optiheap_allocator_init(void);
void* optiheap_allocate(size_t size);
void* optiheap_free(void* ptr);
#ifdef DEBUGGER
void debug_print_heap(int debug_id);
void debug_print_mmap(int debug_id);
#endif

#endif
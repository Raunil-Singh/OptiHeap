#ifndef OPTIHEAP
#define OPTIHEAP

#include <stddef.h>

void optiheap_allocator_init(void);
void* optiheap_allocate(size_t size);
void* optiheap_free(void* ptr);
void debug_print_heap(int debug_id);
void debug_print_mmap(int debug_id);
void* optiheap_reference_allocate(size_t size, void (*destructor)(void *));
void optiheap_retain(void *ptr);
void* optiheap_release(void *ptr);
size_t optiheap_reference_count(void *ptr);
int optiheap_verify_reference_counting(void);

#endif // OPTIHEAP
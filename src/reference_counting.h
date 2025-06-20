#ifndef REFERENCE_COUNTING_H
#define REFERENCE_COUNTING_H

#include <stddef.h>
#include "memory_structs.h"

void optiheap_retain(void *ptr);
void* optiheap_release(void *ptr);
size_t optiheap_reference_count(void *ptr);
void optiheap_set_destructor(void *ptr, void (*destructor)(void *));
int optiheap_verify_reference_counting(void);

#endif // REFERENCE_COUNTING_H
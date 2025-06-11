#include "mmap_allocator.h"
#include "heap_allocator.h"

/*
 * This file implements the optiheap allocator, which is a memory allocator
 * that optimizes memory usage by combining mmap and heap allocation strategies.
 * It uses mmap for large allocations and a heap allocator for smaller ones.
 * 
 * It mainly works as an orchestrator between the mmap allocator and the heap allocator,
 * delegating allocation and deallocation tasks to the appropriate allocator based on
 * the size of the requested memory block.
 */

#define MAX_HEAP_ALLOC_SIZE (1024 * 128) // Define a threshold for heap allocation size

void optiheap_allocator_init()
{
    mmap_allocator_init();
    heap_allocator_init();
}

void* optiheap_allocate(size_t size)
{
    if (size == 0) {
        return NULL; // No allocation for zero size
    }

    if (size > MAX_HEAP_ALLOC_SIZE) {
        return allocate_mmap_block(size); //  for large allocations
    } else {
        return allocate_heap_block(size); //  for smaller allocations
    }
}

void* optiheap_free(void* ptr)
{
    if (!ptr) {
        return NULL; // No action for null pointer
    }

    // This check does not ensure that the pointer is allocated in heap,
    // but it assures that it is not allocated using mmap.
    // Note: A early check and later action continues to remain thread safe,
    // because the heap doesn't shrink at any point in time.
    if(within_heap_range(ptr)) {
        return free_heap_block(ptr);
    } else {
        return free_mmap_block(ptr);
    }
}
#include "mmap_allocator.h"
#include "heap_allocator.h"

#include <stdio.h>


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

    // Use mmap for large allocations (greater than or equal to page size)
    if (size > MAX_HEAP_ALLOC_SIZE) {
        return allocate_mmap_block(size);
    } else {
        // Use heap allocator for smaller allocations
        return allocate_heap_block(size);
    }
}

void* optiheap_free(void* ptr)
{
    if (!ptr) {
        return NULL; // No action for null pointer
    }

    if(ptr >= (void*)heap_list.memory_base && ptr < (void*)heap_list.memory_end) {
        return free_heap_block(ptr);
    } else {
        return free_mmap_block(ptr);
    }
}
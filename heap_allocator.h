#ifndef HEAP_ALLOCATOR_H
#define HEAP_ALLOCATOR_H

#define ALLOCATION_FAILED ((void*)-1)
#define DEALLOCATION_FAILED ((void*)-2)

#include <stddef.h>
#include "memory_structs.h"

struct heap_memory_list {
    struct memory_header *head; // First block in all-blocks list
    struct memory_header *tail; // Last block in all-blocks list
    struct memory_header *free_head; // First block in free list
    struct memory_header *free_tail; // Last block in free list

    // Memory region management
    char *memory_base;
    char *memory_curr;
    char *memory_end;
    size_t memory_size;
};

extern struct heap_memory_list heap_list;

void heap_allocator_init(void);
void* allocate_heap_block(size_t size);
void* free_heap_block(void* ptr);
int within_heap_range(void *ptr);
void debug_print_heap(int debug_id);

#endif // HEAP_ALLOCATOR_H
#ifndef HEAP_ALLOCATOR_H
#define HEAP_ALLOCATOR_H

#define ALLOCATION_FAILED ((void*)-1)
#define DEALLOCATION_FAILED ((void*)-2)

#include <stddef.h>
#include "memory_structs.h"

#define NUM_SIZE_CLASSES 11 // Number of free lists for different sizes

// Class sizes: 0: 96, 1: 192, 2: 384, 3: 768, 4: 1536, 5: 3072, 6: 6144, 7: 12288, 8: 24576, 9: 49152, 10: 98304

struct heap_memory_list {
    struct memory_header *head; // First block in all-blocks list
    struct memory_header *tail; // Last block in all-blocks list
    struct memory_header *free_head[NUM_SIZE_CLASSES]; // First blocks in free lists
    struct memory_header *free_tail[NUM_SIZE_CLASSES]; // Last blocks in free lists

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
#ifndef MMAP_ALLOCATOR_H
#define MMAP_ALLOCATOR_H

#define ALLOCATION_FAILED ((void*)-1)
#define DEALLOCATION_FAILED ((void*)-2)

#include <stddef.h>

struct mmap_memory_list {
    size_t page_size;
    struct memory_header *head; // First block in list
    struct memory_header *tail; // Last block in list
};

extern struct mmap_memory_list mmap_list;

void mmap_allocator_init(void);
void* allocate_mmap_block(size_t size);
void* free_mmap_block(void* ptr);
void debug_print_mmap(int debug_id);

#endif // MMAP_ALLOCATOR_H
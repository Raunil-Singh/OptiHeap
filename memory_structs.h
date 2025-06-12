#ifndef MEMORY_STRUCTS_H
#define MEMORY_STRUCTS_H

#include <stddef.h>
#include <stdint.h>

#define HEAP_FREED 0xDEADBEEF
#define HEAP_ALLOCATED 0xCAFEBABE
#define MMAP_FREED 0xFEEDFACE // this is not really used, but kept for consistency
#define MMAP_ALLOCATED 0xBEEFCAFE

struct memory_header {
    size_t size; // Size of the block
    uint32_t magic; // Magic number for allocation status and validation
    struct memory_header *next; // Next in all-blocks list
    struct memory_header *prev; // Prev in all-blocks list
    struct memory_header *next_free; // Next in free list
    struct memory_header *prev_free; // Prev in free list
};

#endif // MEMORY_STRUCTS_H
#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#define HEAP_FREED 0xDEADBEEF
#define HEAP_ALLOCATED 0xCAFEBABE

struct memory_header {
    size_t size; // Size of the block
    int_32_t magic; // Magic number for allocation status and validation
    struct memory_header *next; // Next in all-blocks list
    struct memory_header *prev; // Prev in all-blocks list
    struct memory_header *next_free; // Next in free list
    struct memory_header *prev_free; // Prev in free list
};

struct memory_list {
    size_t total_size; // Total size of the heap
    size_t used_size; // Size currently used in the heap
    struct memory_header *head; // First block in all-blocks list
    struct memory_header *tail; // Last block in all-blocks list
    struct memory_header *free_head; // First block in free list
    struct memory_header *free_tail; // Last block in free list

    // Heap region management
    char *heap_base;
    char *heap_curr;
    char *heap_end;
    size_t heap_size;
};

struct memory_list heap_list;

// Efficient heap growth: double size when needed
void* try_heap_allocation(size_t block_size)
{
    if (!heap_list.heap_base || (size_t)(heap_list.heap_end - heap_list.heap_curr) < block_size) {
        size_t curr_size = heap_list.heap_size;
        size_t new_size = 2 * curr_size + 1;
        if (new_size < curr_size + block_size) {
            new_size = curr_size + block_size;
        }
        void* block = sbrk(new_size - curr_size);
        if (block == (void *)-1) {
            fprintf(stderr, "Error: sbrk failed to allocate %zu bytes\n", new_size - curr_size);
            _exit(1);
        }
        if (!heap_list.heap_base) {
            heap_list.heap_base = heap_list.heap_curr = block;
        }
        heap_list.heap_end = heap_list.heap_base + new_size;
        heap_list.heap_size = new_size;
        heap_list.total_size = heap_list.heap_size;
    }
    void* result = heap_list.heap_curr;
    heap_list.heap_curr += block_size;
    heap_list.used_size += block_size;
    return result;
}

// Insert a block into the free list (at the tail)
void insert_into_free_list(struct memory_header *block) {
    block->next_free = NULL;
    block->prev_free = heap_list.free_tail;
    if (heap_list.free_tail) {
        heap_list.free_tail->next_free = block;
    }
    heap_list.free_tail = block;
    if (!heap_list.free_head) {
        heap_list.free_head = block;
    }
}

// Remove a block from the free list
void remove_from_free_list(struct memory_header *block) {
    if (block->prev_free) {
        block->prev_free->next_free = block->next_free;
    } else {
        heap_list.free_head = block->next_free;
    }
    if (block->next_free) {
        block->next_free->prev_free = block->prev_free;
    } else {
        heap_list.free_tail = block->prev_free;
    }
    block->next_free = block->prev_free = NULL;
}

// Allocate a block from the heap or free list
void* allocate_heap_block(size_t requested_size)
{
    if (requested_size == 0 || requested_size > SIZE_MAX - sizeof(struct memory_header)) {
        fprintf(stderr, "Error: Invalid size requested: %zu\n", requested_size);
        return NULL; // Invalid size
    }

    // Align requested size to sizeof(struct memory_header)
    size_t aligned_size = (requested_size + sizeof(struct memory_header) - 1) & ~(sizeof(struct memory_header) - 1);

    // First-fit search in free list
    struct memory_header *curr = heap_list.free_head;
    while (curr) {
        if (curr->free && curr->size >= aligned_size) {
            curr->free = false;
            remove_from_free_list(curr);
            return (void *)(curr + 1);
        }
        curr = curr->next_free;
    }

    // No suitable free block, allocate new
    struct memory_header *new_block = try_heap_allocation(sizeof(struct memory_header) + aligned_size);
    new_block->size = aligned_size;
    new_block->free = false;
    new_block->next = NULL;
    new_block->prev = heap_list.tail;
    new_block->next_free = NULL;
    new_block->prev_free = NULL;

    if (heap_list.tail) {
        heap_list.tail->next = new_block;
    }
    heap_list.tail = new_block;
    if (!heap_list.head) {
        heap_list.head = new_block;
    }

    return (void *)(new_block + 1); // Return pointer to the data area
}

// Free a block and add it to the free list
void free_heap_block(void *ptr)
{
    if (!ptr) return;
    struct memory_header *block = ((struct memory_header *)ptr) - 1;
    if (block->free) return; // Already free
    block->free = true;
    insert_into_free_list(block);
}

// Initialize the heap allocator
void heap_allocator_init()
{
    memset(&heap_list, 0, sizeof(struct memory_list));
}
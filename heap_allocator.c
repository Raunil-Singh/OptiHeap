#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#define HEAP_FREED 0xDEADBEEF
#define HEAP_ALLOCATED 0xCAFEBABE

struct memory_header {
    size_t size; // Size of the block
    int32_t magic; // Magic number for allocation status and validation
    struct memory_header *next; // Next in all-blocks list
    struct memory_header *prev; // Prev in all-blocks list
    struct memory_header *next_free; // Next in free list
    struct memory_header *prev_free; // Prev in free list
};

struct memory_list {
    struct memory_header *head; // First block in all-blocks list
    struct memory_header *tail; // Last block in all-blocks list
    struct memory_header *free_head; // First block in free list
    struct memory_header *free_tail; // Last block in free list

    // Memory region management
    char *heap_base;
    char *heap_curr;
    char *heap_end;
    size_t heap_size;
};

struct memory_list heap_list;

/*
 * This function attempts to allocate a block of memory from the heap.
 * If the current heap does not have enough space, it will double the size of the heap
 * until it can accommodate the requested block size.
 * It uses sbrk to request more memory from the system. 
 */
void* try_heap_allocation(size_t block_size)
{
    if (!heap_list.heap_base || (size_t)(heap_list.heap_end - heap_list.heap_curr) < block_size) {
        size_t curr_size = heap_list.heap_size;
        size_t new_size = 2 * (curr_size + block_size);
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
    }
    void* result = heap_list.heap_curr;
    heap_list.heap_curr += block_size;
    return result;
}

/*
 * This function inserts a block into the free list at the tail.
 * It updates the pointers accordingly to maintain the doubly linked list structure.
 */
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

/*
 * This function removes a block from the free list.
 * It updates the pointers accordingly to maintain the doubly linked list structure.
 * If the block is the head or tail of the free list, it updates those pointers as well.
 * It sets the next_free and prev_free pointers of the block to NULL since it is no longer a part of free list.
 */
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

/*
 * This function coalesces adjacent free blocks in the heap.
 * It checks both the previous and next blocks to see if they are free.
 * If they are, it merges them into a single block and updates the free list accordingly.
 * It also ensures that the merged block is inserted back into the free list.
 * This is important for efficient memory management and to reduce fragmentation.
 */
void coalesce_free_blocks(struct memory_header *block) {
    
    // Check and merge with previous block if it is free
    if (block->prev && block->prev->magic == HEAP_FREED) {
        struct memory_header *prev = block->prev;
        remove_from_free_list(prev);

        prev->size += sizeof(struct memory_header) + block->size;
        prev->next = block->next;
        if (block->next) {
            block->next->prev = prev;
        } else {
            heap_list.tail = prev;
        }
        block = prev;
    }

    // Check and merge with next block if it is free
    if (block->next && block->next->magic == HEAP_FREED) {
        struct memory_header *next = block->next;
        remove_from_free_list(next);

        block->size += sizeof(struct memory_header) + next->size;
        block->next = next->next;
        if (next->next) {
            next->next->prev = block;
        } else {
            heap_list.tail = block;
        }
    }

    insert_into_free_list(block);
}
 
/*
 * This function allocates a block of memory from the heap.
 * It first checks the free list for a suitable block using a best-fit strategy.
 * If a suitable block is found, it splits the block if it is much larger than needed.
 * If no suitable block is found, it attempts to allocate a new block from the heap using sbrk.
 * It returns a pointer to the allocated memory, or NULL if allocation fails.
 * The function also handles alignment of the requested size to ensure proper memory alignment.
 */
void* allocate_heap_block(size_t requested_size)
{
    if (requested_size == 0) {
        return NULL; // No allocation for zero size
    }

    // Align requested size to sizeof(struct memory_header)
    size_t aligned_blocks = (requested_size + sizeof(struct memory_header) - 1) / sizeof(struct memory_header);
    
    // The extra one is for the header to store metadata
    size_t aligned_size = (aligned_blocks + 1) * sizeof(struct memory_header);

    // Best-fit search in free list
    struct memory_header *best_fit = NULL;
    size_t best_fit_size = SIZE_MAX;
    struct memory_header *curr = heap_list.free_head;
    while (curr) {
        if (curr->magic == HEAP_FREED && curr->size >= aligned_size) {
            if (curr->size < best_fit_size) {
                best_fit = curr;
                best_fit_size = curr->size;
            }
        }
        curr = curr->next_free;
    }

    if (best_fit) {
        size_t excess = best_fit->size - aligned_size;
        
        // if there's excess, we split the block to use the excess space later
        if (excess) {

            // Modify the best_fit block
            remove_from_free_list(best_fit); // Remove from free list
            best_fit->magic = HEAP_ALLOCATED; // Mark as allocated
            best_fit->size = aligned_size; // Set the size of the allocated block
            
            // Create a new free block for the excess space
            struct memory_header *new_free = (struct memory_header *)((char *)(best_fit + 1) + aligned_size);
            new_free->size = excess - sizeof(struct memory_header);
            new_free->magic = HEAP_FREED;
            new_free->next = best_fit->next;
            new_free->prev = best_fit;
            new_free->next_free = NULL;
            new_free->prev_free = NULL;

            insert_into_free_list(new_free);

            best_fit->next = new_free;

            if (best_fit->next) {
                best_fit->next->prev = new_free;
            } else {
                heap_list.tail = new_free;
            }
            best_fit->next = new_free;
        }
        else {
            // execution comes here if no excess space
            best_fit->magic = HEAP_ALLOCATED;
            remove_from_free_list(best_fit);
        }
        return (void *)(best_fit + 1);
    }

    // No suitable free block, allocate new
    struct memory_header *new_block = (struct memory_header *)try_heap_allocation(sizeof(struct memory_header) + aligned_size);
    new_block->magic = HEAP_ALLOCATED;
    new_block->size = aligned_size;
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

/*
 * This function frees a previously allocated block of memory.
 * It checks if the pointer is valid and if the block is currently allocated.
 * If valid, it marks the block as free and attempts to coalesce it with adjacent free blocks.
 * It also updates the free list accordingly.
 */
void free_heap_block(void *ptr)
{
    if (!ptr) return;
    struct memory_header *block = ((struct memory_header *)ptr) - 1;

    if ((void*)(block) < (void*)(heap_list.heap_base) || (void*)(block) >= (void*)(heap_list.heap_end)) {
        fprintf(stderr, "Error: Attempt to free invalid pointer %p\n", ptr);
        _exit(1); // Invalid pointer
    }
    
    if (block->magic != HEAP_ALLOCATED) {
        fprintf(stderr, "Error: Attempt to free invalid or corrupted pointer %p\n", ptr);
        _exit(1);
    }

    block->magic = HEAP_FREED; // This helps to identify the block as free
    
    // Coalesce with adjacent free blocks and insert into free list
    coalesce_free_blocks(block);
}

/*
 * This function initializes the heap allocator.
 * It sets the initial state of the heap list, including the head and tail pointers,
 * and the base, current, and end pointers of the heap.
 */
void heap_allocator_init()
{
    memset(&heap_list, 0, sizeof(struct memory_list));
}

int main() {
    heap_allocator_init();

    int* arr = (int*)allocate_heap_block(10 * sizeof(int));
    for(int i = 0; i < 10; i++) {
        arr[i] = i;
    }
    for(int i = 0; i < 100; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    free_heap_block(arr);   

    return 0;
}
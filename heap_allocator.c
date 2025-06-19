#include "memory_structs.h"
#include "heap_allocator.h"

#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>

#define GROWTH_FACTOR 3

void *sbrk(intptr_t increment); 
 
struct heap_memory_list heap_list;

#ifdef THREAD_SAFE
static pthread_mutex_t heap_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/*
 * This function initializes the heap allocator.
 * It sets the initial state of the heap list, including the head and tail pointers,
 * and the base, current, and end pointers of the heap.
 */
void heap_allocator_init()
{
    #ifdef THREAD_SAFE
    pthread_mutex_lock(&heap_mutex);
    #endif
    memset(&heap_list, 0, sizeof(struct heap_memory_list));
    #ifdef THREAD_SAFE
    pthread_mutex_unlock(&heap_mutex);
    #endif
}


/*
 * This function checks if a pointer is within the range of the heap memory.
 * It returns 1 if the pointer is within the heap range, otherwise returns 0.
 */
int within_heap_range(void *ptr)
{
    #ifdef THREAD_SAFE
    pthread_mutex_lock(&heap_mutex);
    #endif
    int result = 0;
    if (ptr >= (void *)heap_list.memory_base && ptr < (void *)heap_list.memory_end) {
        result = 1;
    }
    #ifdef THREAD_SAFE
    pthread_mutex_unlock(&heap_mutex);
    #endif
    return result;
}


/*
 * This function attempts to allocate a block of memory from the heap.
 * If the current heap does not have enough space, it will double the size of the heap
 * until it can accommodate the requested block size.
 * It uses sbrk to request more memory from the system. 
 */
void* try_heap_allocation(size_t block_size)
{
    if (!heap_list.memory_base || (size_t)(heap_list.memory_end - heap_list.memory_curr) < block_size) {
        size_t curr_size = heap_list.memory_size;
        size_t new_size = GROWTH_FACTOR * (curr_size + block_size);
        if (new_size < curr_size + block_size) {
            new_size = curr_size + block_size;
        }
        void* block = (void *)sbrk(new_size - curr_size);
        if (block == (void *)-1) {
            fprintf(stderr, "Error: sbrk failed to allocate %zu bytes\n", new_size - curr_size);
            return ALLOCATION_FAILED; // Indicate failure
        }
        if (!heap_list.memory_base) {
            heap_list.memory_base = heap_list.memory_curr = block;
        }
        heap_list.memory_end = heap_list.memory_base + new_size;
        heap_list.memory_size = new_size;
    }
    void* result = heap_list.memory_curr;
    heap_list.memory_curr += block_size;
    return result;
}


/*
 * This function calculates the size class for a given size.
 * It determines which free list the block should belong to based on its size.
 * The size classes are defined in a way that each class can hold blocks of a certain size range.
 */
size_t get_size_class(size_t size) {
    size_t size_class = 0;
    while(size_class < NUM_SIZE_CLASSES && size > ((sizeof(struct memory_header)*2) << (size_class+1))) {
        size_class++;
    }
    if (size_class >= NUM_SIZE_CLASSES) {
        size_class = NUM_SIZE_CLASSES - 1; // Cap at the last size class
    }
    return size_class;
}


/*
 * This function inserts a block into the free list at the tail.
 * It updates the pointers accordingly to maintain the doubly linked list structure.
 */
void insert_into_free_list(struct memory_header *block) {
    size_t class = get_size_class(block->size);
    block->next_free = NULL;
    block->prev_free = heap_list.free_tail[class];
    if (heap_list.free_tail[class]) {
        heap_list.free_tail[class]->next_free = block;
    }
    heap_list.free_tail[class] = block;
    if (!heap_list.free_head[class]) {
        heap_list.free_head[class] = block;
    }
}


/*
 * This function removes a block from the free list.
 * It updates the pointers accordingly to maintain the doubly linked list structure.
 * If the block is the head or tail of the free list, it updates those pointers as well.
 * It sets the next_free and prev_free pointers of the block to NULL since it is no longer a part of free list.
 */
void remove_from_free_list(struct memory_header *block) {
    size_t class = get_size_class(block->size);
    if (block->prev_free) {
        block->prev_free->next_free = block->next_free;
    } else {
        heap_list.free_head[class] = block->next_free;
    }
    if (block->next_free) {
        block->next_free->prev_free = block->prev_free;
    } else {
        heap_list.free_tail[class] = block->prev_free;
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

    void * allocation_ptr = NULL; // store the pointer to the allocated memory

    // Align requested size to sizeof(struct memory_header)
    size_t aligned_blocks = (requested_size + sizeof(struct memory_header) - 1) / sizeof(struct memory_header);
    size_t aligned_size = (aligned_blocks) * sizeof(struct memory_header);

    #ifdef THREAD_SAFE
    pthread_mutex_lock(&heap_mutex);
    #endif

    // The use of extra sizeof(struct memory_header) helps in ensuring that
    // the first block that we encounter in the free list is large enough
    size_t class = get_size_class(aligned_size) + 1;

    struct memory_header *first_fit = NULL;

    // First-fit search: look for the first block in any suitable class
    for (size_t c = class; c < NUM_SIZE_CLASSES; ++c) {
        if (!heap_list.free_head[c]) continue;
        first_fit = heap_list.free_head[c];
        break;
    }

    if (first_fit) {
        size_t excess = first_fit->size - aligned_size;
        
        remove_from_free_list(first_fit); // Remove from free list
        first_fit->magic = HEAP_ALLOCATED; // Mark as allocated
        
        // if there's excess, we split the block to use the excess space later
        if (excess > 2*sizeof(struct memory_header)) {

            // Modify the best_fit block
            first_fit->size = aligned_size; // Set the size of the allocated block
            
            // Create a new free block for the excess space
            struct memory_header *new_free = (struct memory_header *)((char *)(first_fit) + sizeof(struct memory_header) + aligned_size);
            memset(new_free, 0, sizeof(struct memory_header)); // Initialize the new block
            new_free->size = excess - sizeof(struct memory_header);
            new_free->magic = HEAP_FREED;
            new_free->next = first_fit->next;
            new_free->prev = first_fit;
            
            insert_into_free_list(new_free);

            if (first_fit->next) {
                first_fit->next->prev = new_free;
            } else {
                heap_list.tail = new_free;
            }
            first_fit->next = new_free;
        }

        allocation_ptr = (void *)(first_fit + 1);
        goto END;
    }

    // No suitable free block, allocate new
    struct memory_header *new_block = (struct memory_header *)try_heap_allocation(aligned_size + sizeof(struct memory_header));
    
    if(new_block == ALLOCATION_FAILED) {
        fprintf(stderr, "Error: Unable to allocate %zu bytes from heap\n", aligned_size + sizeof(struct memory_header));
        allocation_ptr = ALLOCATION_FAILED; // Allocation failed
        goto END;
    }
    
    memset(new_block, 0, sizeof(struct memory_header)); // Initialize the new block
    new_block->magic = HEAP_ALLOCATED;
    new_block->size = aligned_size; // Set the size of the allocated block
    new_block->prev = heap_list.tail;



    if (heap_list.tail) {
        heap_list.tail->next = new_block;
    }
    heap_list.tail = new_block;
    if (!heap_list.head) {
        heap_list.head = new_block;
    }

    allocation_ptr = (void *)(new_block + 1); // Return pointer to the data area

    END:
    #ifdef THREAD_SAFE
    pthread_mutex_unlock(&heap_mutex);
    #endif
    return allocation_ptr;
}


/*
 * This function frees a previously allocated block of memory.
 * It checks if the pointer is valid and if the block is currently allocated.
 * If valid, it marks the block as free and attempts to coalesce it with adjacent free blocks.
 * It also updates the free list accordingly.
 */
void* free_heap_block(void *ptr)
{
    if (!ptr) return NULL;

    struct memory_header *block = ((struct memory_header *)ptr) - 1;

    void * status = NULL; // store the status of deallocation

    if (!within_heap_range(block)) {
        fprintf(stderr, "Error: Attempt to free pointer %p in unallocated regions\n", ptr);
        status = DEALLOCATION_FAILED; // Invalid pointer
        return status;
    }

    #ifdef THREAD_SAFE
    pthread_mutex_lock(&heap_mutex);
    #endif

    if (block->magic != HEAP_ALLOCATED) {
        fprintf(stderr, "Error: Magic Number -> %x, expected %x for pointer %p\n", 
                block->magic, HEAP_ALLOCATED, ptr);
        fprintf(stderr, "Error: Attempt to free invalid or corrupted pointer %p\n", ptr);
        status = DEALLOCATION_FAILED;
        goto END;
    }

    block->magic = HEAP_FREED; // This helps to identify the block as free
    
    // Coalesce with adjacent free blocks and insert into free list
    coalesce_free_blocks(block);
    status = NULL;

    END:
    #ifdef THREAD_SAFE
    pthread_mutex_unlock(&heap_mutex);
    #endif
    return status; // Return NULL on successful deallocation, or DEALLOCATION_FAILED on error
}


void debug_print_heap([[maybe_unused]]int debug_id)
{
    #ifdef OPTIHEAP_DEBUGGER
    #ifdef THREAD_SAFE
    pthread_mutex_lock(&heap_mutex);
    #endif
    struct memory_header *curr = heap_list.head;
    printf("================================================================= START DEBUG_ID : %d\n", debug_id);
    printf("Heap Memory State:\n");
    printf("Heap Size: %zu bytes\n", heap_list.memory_size);
    printf("Heap Start: %p - Heap End: %p\n", heap_list.memory_base, heap_list.memory_end);
    while (curr) {
        printf("Block at %p: \t State=%s \tdata_size=%zu, total_size=%zu\n",
            (void*)curr,
            curr->magic == HEAP_ALLOCATED ? "ALLOCATED" :
            (curr->magic == HEAP_FREED ? "  FREE   " : "CORRUPTED"),
            curr->size,
            curr->size + sizeof(struct memory_header));
        curr = curr->next;
    }
    printf("================================================================= END DEBUG_ID : %d\n", debug_id);
    #ifdef THREAD_SAFE
    pthread_mutex_unlock(&heap_mutex);
    #endif
    #else
    printf("Warning: OptiHeap Debugger is disabled. Enable it by compiling with -DOPTIHEAP_DEBUGGER flag to see heap state.\n");
    #endif
}

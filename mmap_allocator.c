#include "memory_structs.h"
#include "mmap_allocator.h"

#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

struct mmap_memory_list mmap_list;

#ifdef THREAD_SAFE
static pthread_mutex_t mmap_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/*
 * This function initializes the mmap allocator.
 * It sets the initial state of the mmap list, including the head and tail pointers,
 * and the page size for memory allocation.
 */

void mmap_allocator_init() {
    #ifdef THREAD_SAFE
    pthread_mutex_lock(&mmap_mutex);
    #endif
    memset(&mmap_list, 0, sizeof(struct mmap_memory_list));
    mmap_list.page_size = sysconf(_SC_PAGESIZE);
    #ifdef THREAD_SAFE
    pthread_mutex_unlock(&mmap_mutex);
    #endif
}


/*
 * Insert a block into the mmap list.
 * This function adds the block to the end of the list.
 * It assumes that the block is already allocated and has a valid header.
 */
void insert_into_mmap_list(struct memory_header *block) {
    block->next = NULL;
    block->prev = mmap_list.tail;
    if (mmap_list.tail) {
        mmap_list.tail->next = block;
    }
    mmap_list.tail = block;
    if (!mmap_list.head) {
        mmap_list.head = block;
    }
}


/*
 * Remove a block from the mmap list.
 * This function removes the specified block from the list.
 * It updates the head and tail pointers as necessary.
 */
void remove_from_mmap_list(struct memory_header *block) {
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        mmap_list.head = block->next;
    }
    if (block->next) {
        block->next->prev = block->prev;
    } else {
        mmap_list.tail = block->prev;
    }
}


/*
 * Allocate a memory block using mmap.
 * This function allocates a block of memory of at least the requested size,
 * aligned to the page size, and returns a pointer to the usable memory area.
 */
void* allocate_mmap_block(size_t requested_size)
{
    if (requested_size == 0)
    {
        return NULL;
    }

    #ifdef THREAD_SAFE
    pthread_mutex_lock(&mmap_mutex);
    #endif

    void * allocation_ptr = NULL;;

    // Here bitwise operations are used to align the size to the page size because page size is a power of two.
    size_t aligned_size = (requested_size + sizeof(struct memory_header) + mmap_list.page_size - 1) & ~(mmap_list.page_size - 1);
    
    struct memory_header *new_block = mmap(NULL, aligned_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (new_block == MAP_FAILED)
    {
        fprintf(stderr, "Error: mmap failed to allocate %zu bytes\n", aligned_size);
        allocation_ptr = ALLOCATION_FAILED;
        goto END;
    }

    memset(new_block, 0, sizeof(struct memory_header));
    new_block->magic = MMAP_ALLOCATED;
    new_block->size = aligned_size - sizeof(struct memory_header); // Store the size excluding the header

    if (!mmap_list.head)
    {
        mmap_list.head = new_block;
    }

    insert_into_mmap_list(new_block);

    allocation_ptr = (void *)(new_block + 1);

    END:
    #ifdef THREAD_SAFE
    pthread_mutex_unlock(&mmap_mutex);
    #endif
    return allocation_ptr; 
}


/*
 * Free a memory block allocated with mmap.
 * This function removes the block from the mmap list and deallocates the memory.
 * returns NULL if deallocation is successful
 * returns DEALLOCATION_FAILED if deallocation fails
 */
void* free_mmap_block(void *ptr)
{
    if (!ptr) 
    {
        return NULL; // No action for null pointer
    }

    void * status = NULL; // Default to NULL for successful deallocation
    struct memory_header *block = (struct memory_header *)ptr - 1; // Get the header from the pointer

    #ifdef THREAD_SAFE
    pthread_mutex_lock(&mmap_mutex);
    #endif

    #ifdef OPTIHEAP_DEBUGGER

    struct memory_header *curr = mmap_list.head;
    while(curr && curr != block) {
        curr = curr->next;
    }

    if(curr) {
        if (curr->magic != MMAP_ALLOCATED) {
            fprintf(stderr, "Error: Attempt to free a block that is not allocated or has been corrupted.\n");
            #ifdef THREAD_SAFE
            pthread_mutex_unlock(&mmap_mutex);
            #endif
            status = DEALLOCATION_FAILED;
            goto END;
        }

        // Remove the block from the mmap list
        remove_from_mmap_list(curr);

        // Unmap the memory
        if (munmap(curr, curr->size + sizeof(struct memory_header)) == -1) {
            fprintf(stderr, "Error: munmap failed to deallocate memory.\n");
            status =  DEALLOCATION_FAILED; // Indicate failure
            goto END;
        }
    }
    else status = DEALLOCATION_FAILED;
    
    #else
    
    remove_from_mmap_list(block);
    if (munmap(block, block->size + sizeof(struct memory_header)) == -1) {
        status =  DEALLOCATION_FAILED;
        goto END;
    }

    #endif

    END:
    #ifdef THREAD_SAFE
    pthread_mutex_unlock(&mmap_mutex);
    #endif
    return status;
}


void debug_print_mmap([[maybe_unused]]int debug_id)
{
    #ifdef OPTIHEAP_DEBUGGER
    #ifdef THREAD_SAFE
    pthread_mutex_lock(&mmap_mutex);
    #endif
    struct memory_header *curr = mmap_list.head;
    printf("================================================================= START DEBUG_ID : %d\n", debug_id);
    printf("MMapped Memory State:\n");
    while (curr) {
        printf("Block at %p: \t State=%s \tdata_size=%zu, total_size=%zu\n",
            (void*)curr,
            curr->magic == MMAP_ALLOCATED ? "ALLOCATED" : "CORRUPTED",
            curr->size,
            curr->size + sizeof(struct memory_header));
        curr = curr->next;
    }
    printf("================================================================= END DEBUG_ID : %d\n", debug_id);
    #ifdef THREAD_SAFE
    pthread_mutex_unlock(&mmap_mutex);
    #endif
    #else
    printf("Warning: OptiHeap Debugger is disabled. Enable it by compiling with -DOPTIHEAP_DEBUGGER flag to see mmap state.\n");
    #endif
}
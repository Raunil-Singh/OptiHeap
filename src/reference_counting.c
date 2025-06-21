#include "memory_structs.h"
#include "reference_counting.h"
#include "heap_allocator.h"
#include "mmap_allocator.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

/*
 * This function locks the appropriate mutex based on the type of memory block (heap or mmap).
 * It returns a pointer to the mutex that was locked.
 */
pthread_mutex_t * optiheap_lock([[maybe_unused]]struct memory_header *block)
{
    pthread_mutex_t *mutex = NULL;
    #ifdef OPTIHEAP_THREAD_SAFE
    if(block->magic == HEAP_ALLOCATED) {
        mutex = &heap_mutex;
    }
    else {
        mutex = &mmap_mutex;
    }
    pthread_mutex_lock(mutex);
    #endif
    return mutex;
}


/*
 * This function unlocks the mutex that was previously locked by optiheap_lock.
 * It takes a pointer to the mutex as an argument.
 */
void optiheap_unlock([[maybe_unused]]pthread_mutex_t *mutex)
{
    #ifdef OPTIHEAP_THREAD_SAFE
    pthread_mutex_unlock(mutex);
    #endif
}


/*
 * This function retains a pointer to a previously allocated block of memory.
 * It increments the reference count of the block to indicate the increase in ownership.
 */
void optiheap_retain([[maybe_unused]]void *ptr)
{
    #ifdef OPTIHEAP_REFERENCE_COUNTING
    struct memory_header *block = (struct memory_header *)ptr - 1;

    #ifdef OPTIHEAP_DEBUGGER
    if(!within_heap_range(block) && !present_in_mmap_list(block)) {
        printf("Error: Invalid pointer %p passed to optiheap_retain.\nThe address might already be freed or not allocated at all.\n", ptr);
        return;
    }
    if(block->magic != HEAP_ALLOCATED && block->magic != MMAP_ALLOCATED) {
        printf("Error: Invalid pointer %p passed to optiheap_retain.\nThe address might already be freed, got corrupted, or not allocated at all.\n", ptr);
        return;
    }
    #endif

    pthread_mutex_t *mutex = optiheap_lock(block);

    #ifdef OPTIHEAP_DEBUGGER
    if (block->ref_count == SIZE_MAX){
        printf("Error: Reference count overflow for pointer %p.\n", ptr);
        #ifdef OPTIHEAP_THREAD_SAFE
        optiheap_unlock(mutex);
        #endif
        return;
    }
    #endif
    
    block->ref_count++;

    #ifdef OPTIHEAP_DEBUGGER
    printf("Retained pointer %p, new reference count: %zu.\n", ptr, block->ref_count);
    #endif
    
    optiheap_unlock(mutex);
    #else
    fprintf(stderr, "Error: Reference counting is not enabled. Compile with -DOPTIHEAP_REFERENCE_COUNTING to enable it.\n");
    #endif
}


/*
 * This function releases a pointer to a previously allocated block of memory.
 * It decrements the reference count of the block and frees it if the count reaches zero.
 */
void * optiheap_release([[maybe_unused]]void *ptr)
{
    #ifdef OPTIHEAP_REFERENCE_COUNTING
    struct memory_header *block = (struct memory_header *)ptr - 1;

    #ifdef OPTIHEAP_DEBUGGER
    if(!within_heap_range(block) && !present_in_mmap_list(block)) {
        printf("Error: Invalid pointer %p passed to optiheap_release.\nThe address might already be freed or not allocated at all.\n", ptr);
        return (void *)-1; // Return -1 to indicate an error
    }
    if(block->magic != HEAP_ALLOCATED && block->magic != MMAP_ALLOCATED) {
        printf("Error: Invalid pointer %p passed to optiheap_release.\nThe address might already be freed, got corrupted, or not allocated at all.\n", ptr);
        return (void *)-1; // Return -1 to indicate an error
    }
    #endif

    optiheap_lock(block);

    block->ref_count--;
    
    #ifdef OPTIHEAP_DEBUGGER
    printf("Released pointer %p, new reference count: %zu.\n", ptr, block->ref_count);
    #endif

    if (block->ref_count == 0) {
        if(block->destructor) block->destructor(ptr); // Call the destructor if it exists
        if(block->magic == HEAP_ALLOCATED) {
            return free_heap_block(ptr);
        } else if(block->magic == MMAP_ALLOCATED) {
            return free_mmap_block(ptr);
        }
    }

    #ifdef OPTIHEAP_THREAD_SAFE
    pthread_mutex_unlock(mutex);
    return NULL;
    #endif
    #else
    fprintf(stderr, "Error: Reference counting is not enabled. Compile with -DOPTIHEAP_REFERENCE_COUNTING to enable it.\n");
    return (void *)-1; // Return -1 to indicate an error
    #endif
}


/*
 * This function checks if a pointer is retained, i.e., if its reference count is greater than zero.
 * It returns 1 if the pointer is retained, otherwise returns 0.
 */
size_t optiheap_reference_count([[maybe_unused]]void *ptr)
{
    #ifdef OPTIHEAP_REFERENCE_COUNTING
    struct memory_header *block = (struct memory_header *)ptr - 1;

    #ifdef OPTIHEAP_DEBUGGER
    if(!within_heap_range(block) && !present_in_mmap_list(block)) {
        printf("Error: Invalid pointer %p passed to optiheap_reference_count.\nThe address might already be freed or not allocated at all.\n", ptr);
        return 0;
    }
    if(block->magic != HEAP_ALLOCATED && block->magic != MMAP_ALLOCATED) {
        printf("Error: Invalid pointer %p passed to optiheap_reference_count.\nThe address might already be freed, got corrupted, or not allocated at all.\n", ptr);
        return 0;
    }
    #endif

    pthread_mutex_t *mutex = optiheap_lock(block);
    size_t ref_count = block->ref_count;
    optiheap_unlock(mutex);

    return block->ref_count;
    #else
    fprintf(stderr, "Error: Reference counting is not enabled. Compile with -DOPTIHEAP_REFERENCE_COUNTING to enable it.\n");
    return 0; // Return 0
    #endif
}


/*
 * This function sets a destructor for a pointer to a previously allocated block of memory.
 */
void optiheap_set_destructor([[maybe_unused]]void *ptr, [[maybe_unused]]void (*destructor)(void *))
{
    #ifdef OPTIHEAP_REFERENCE_COUNTING
    struct memory_header *block = (struct memory_header *)ptr - 1;
    block->destructor = destructor;
    #endif
}


/*
 * This function verifies if the reference counting retain and release functions are
 * balanced by the end of the program.
 * It checks if all allocated blocks have a reference count of zero.
 * If any block has a non-zero reference count, it prints an error message.
 * This function is only functional if OPTIHEAP_DEBUGGER is defined.
 * It returns the number of leaks detected.
 */
int optiheap_verify_reference_counting()
{
    int leaks_detected = 0;
    #ifdef OPTIHEAP_REFERENCE_COUNTING
    #ifdef OPTIHEAP_DEBUGGER
    struct memory_header *current = heap_list.head;
    while (current != NULL) {
        if (current->ref_count != 0) {
            printf("Error: Memory leak detected for pointer %p, reference count: %zu.\n", (void *)(current + 1), current->ref_count);
            leaks_detected++;
        }
        current = current->next;
    }
    current = mmap_list.head;
    while (current != NULL) {
        if (current->ref_count != 0) {  
            printf("Error: Memory leak detected for pointer %p, reference count: %zu.\n", (void *)(current + 1), current->ref_count);
            leaks_detected++;
        }
    }
    printf("Reference counting verification complete. %d leaks detected.\n", leaks_detected);
    #else
    printf("Warning: OptiHeap Debugger is disabled. Enable it by compiling with -DOPTIHEAP_DEBUGGER flag to see reference counting verification.\n");
    #endif
    #else
    fprintf(stderr, "Error: Reference counting is not enabled. Compile with -DOPTIHEAP_REFERENCE_COUNTING to enable it.\n");
    #endif
    return leaks_detected;
}
#include "heap_allocator.h"
#include <stdio.h>

int main()
{
    heap_allocator_init();

    printf("Metadata size : %ld\n", sizeof(struct memory_header));

    int debug_id = 0;
    debug_print_heap(debug_id++);

    // Allocate a block of memory
    int *arr = (int *)allocate_heap_block(100 * sizeof(int));
    if (!arr) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    void * a = allocate_heap_block(100 * sizeof(int)); // Allocate a larger block to test the heap expansion
    void * b = allocate_heap_block(50 * sizeof(int)); // Allocate another block to test the heap expansion
    void * c = allocate_heap_block(200 * sizeof(int)); // Allocate another block to test the heap expansion


    // 1
    debug_print_heap(debug_id++);
    // passed


    // Initialize the allocated memory
    for (int i = 0; i < 10; i++) {
        arr[i] = i*i;
    }

    // Print the allocated memory
    for (int i = 0; i < 10; i++) {
        printf("%d ", arr[i]);
    }
    printf("\n");

    // Free the allocated memory
    free_heap_block(arr);

    // 2
    debug_print_heap(debug_id++);
    // passed

    void * d = allocate_heap_block(70 * sizeof(int)); // Allocate another block to test the heap expansion

    // 3
    debug_print_heap(debug_id++);
    // passed

    free_heap_block(a);

    // 4
    debug_print_heap(debug_id++);
    // passed

    free_heap_block(c);

    // 5
    debug_print_heap(debug_id++);
    // passed

    free_heap_block(b);

    // 6
    debug_print_heap(debug_id++);
    // passed

    free_heap_block(d);

    // 7
    debug_print_heap(debug_id++);
    // passed

    d = allocate_heap_block(100000 * sizeof(int)); // Allocate a block after freeing to test the allocator

    // 8
    debug_print_heap(debug_id++);
    // passed

    free_heap_block(d);

    // 9
    debug_print_heap(debug_id++);
    // passed

    return 0;
}
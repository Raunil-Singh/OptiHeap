#include "mmap_allocator.h"
#include <stdio.h>
#include <string.h>

int main()
{
    // Initialize the mmap allocator with a page size of 4096 bytes
    mmap_allocator_init();

    int debug_id = 1; // Example debug ID for tracking
    debug_print_mmap(debug_id++);

    // 2
    void *a = allocate_mmap_block(1024 * 1024 * 1024 * 8ULL); // Allocate 4 GB
    debug_print_mmap(debug_id++);

    // 3
    void *b = allocate_mmap_block(300);
    debug_print_mmap(debug_id++);

    // 4
    void *c = allocate_mmap_block(300);
    debug_print_mmap(debug_id++);

    // 5
    void *d = allocate_mmap_block(300);
    debug_print_mmap(debug_id++);

    // 6
    free_mmap_block(a);
    debug_print_mmap(debug_id++);

    // 7
    free_mmap_block(c);
    debug_print_mmap(debug_id++);

    // 8
    free_mmap_block(NULL);
    debug_print_mmap(debug_id++);

    // 9
    free_mmap_block(a);
    debug_print_mmap(debug_id++);

    printf("All tests passed successfully.\n");

    return 0;
}
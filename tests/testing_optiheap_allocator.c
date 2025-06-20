#include "../include/optiheap_allocator.h"
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

int main() {

    optiheap_allocator_init();

    // 1. Allocate at the threshold boundary (should use heap)
    void *a = optiheap_allocate(1024 * 128);
    assert(a != NULL);
    assert(optiheap_free(a) == NULL);

    // 2. Allocate just above the threshold (should use mmap)
    void *b = optiheap_allocate(1024 * 128 + 1);
    assert(b != NULL);
    assert(optiheap_free(b) == NULL);

    // 3. Allocate just below the threshold (should use heap)
    void *c = optiheap_allocate(1024 * 128 - 1);
    assert(c != NULL);
    assert(optiheap_free(c) == NULL);

    // 4. Free a pointer not allocated by optiheap (should print error, not crash)
    int dummy;
    assert(optiheap_free(&dummy) != NULL);

    // 5. Free a pointer in the middle of a valid allocation (should print error, not crash)
    void *d = optiheap_allocate(100);
    assert(d != NULL);
    void *mid = (char*)d + 10;
    assert(optiheap_free(mid) != NULL);
    assert(optiheap_free(d) == NULL);

    // 6. Double free (should print error, not crash)
    void *e = optiheap_allocate(200000);
    assert(e != NULL);
    assert(optiheap_free(e) == NULL);
    assert(optiheap_free(e) != NULL);

    // 7. Free NULL (should do nothing)
    assert(optiheap_free(NULL) == NULL);

    printf("All edge/robustness tests passed!\n");
    return 0;
}
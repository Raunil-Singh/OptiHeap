#include <stdio.h>
#include <assert.h>
#include "optiheap_allocator.h"

static int destructor_called = 0;

void test_destructor(void *ptr) {
    (void)ptr;
    destructor_called++;
}

void test_basic_retain_release() {
    destructor_called = 0;
    void *ptr = optiheap_reference_allocate(64, test_destructor);
    assert(ptr != NULL);
    assert(optiheap_reference_count(ptr) == 1);

    optiheap_retain(ptr);
    assert(optiheap_reference_count(ptr) == 2);

    optiheap_release(ptr);
    assert(optiheap_reference_count(ptr) == 1);

    optiheap_release(ptr); // Should call destructor
    assert(destructor_called == 1);
}

void test_double_release() {
    destructor_called = 0;
    void *ptr = optiheap_reference_allocate(32, test_destructor);
    assert(ptr != NULL);
    assert(optiheap_reference_count(ptr) == 1);

    optiheap_release(ptr); // Should call destructor
    assert(destructor_called == 1);

    // Double release should not crash or call destructor again
    optiheap_release(ptr);
    assert(destructor_called == 1);
}

void test_multiple_retain_release() {
    destructor_called = 0;
    void *ptr = optiheap_reference_allocate(128, test_destructor);
    assert(ptr != NULL);

    for (int i = 0; i < 10; ++i) {
        optiheap_retain(ptr);
    }
    assert(optiheap_reference_count(ptr) == 11);

    for (int i = 0; i < 10; ++i) {
        optiheap_release(ptr);
    }
    assert(optiheap_reference_count(ptr) == 1);

    optiheap_release(ptr);
    assert(destructor_called == 1);
}

void test_null_pointer() {
    // Should not crash
    optiheap_retain(NULL);
    optiheap_release(NULL);
    assert(optiheap_reference_count(NULL) == 0);
}

void test_interleaved_allocations() {
    destructor_called = 0;
    void *a = optiheap_reference_allocate(16, test_destructor);
    void *b = optiheap_reference_allocate(16, test_destructor);
    assert(a && b);

    optiheap_retain(a);
    optiheap_retain(b);
    assert(optiheap_reference_count(a) == 2);
    assert(optiheap_reference_count(b) == 2);

    optiheap_release(a);
    optiheap_release(b);
    assert(optiheap_reference_count(a) == 1);
    assert(optiheap_reference_count(b) == 1);

    optiheap_release(a);
    optiheap_release(b);
    assert(destructor_called == 2);
}

void test_leak_detection() {
    destructor_called = 0;
    void *ptr = optiheap_reference_allocate(64, test_destructor);
    assert(ptr != NULL);
    // Intentionally do not release
    optiheap_verify_reference_counting(); // Should print a leak warning if OPTIHEAP_DEBUGGER is enabled

    // Clean up
    optiheap_release(ptr);
}

int main() {
    optiheap_allocator_init();

    printf("Running reference counting unit tests...\n");

    test_basic_retain_release();
    printf("test_basic_retain_release passed.\n");

    test_double_release();
    printf("test_double_release passed.\n");

    test_multiple_retain_release();
    printf("test_multiple_retain_release passed.\n");

    test_null_pointer();
    printf("test_null_pointer passed.\n");

    test_interleaved_allocations();
    printf("test_interleaved_allocations passed.\n");

    test_leak_detection();
    printf("test_leak_detection passed.\n");

    printf("All reference counting tests passed!\n");
    return 0;
}
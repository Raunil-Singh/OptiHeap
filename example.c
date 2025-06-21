#include "./include/optiheap_allocator.h"
#include <stdio.h>

int main() {
    optiheap_allocator_init();

    int* ptr = (int*)optiheap_allocate(100);
    if (ptr == NULL) {
        printf("Memory allocation failed\n");
        return 1;
    }

    for(int i = 0 ; i < 100 ; i++) {
        ptr[i] = 100-i;
    }

    for(int i = 0 ; i < 100 ; i++) {
        printf("%d ", ptr[i]);
    }
    printf("\n");
    
    optiheap_free(ptr);

    return 0;
}
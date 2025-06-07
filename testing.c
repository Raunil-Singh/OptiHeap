#include <limits.h>
#include <unistd.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>

int main() {

    size_t num = 16;
    scanf("%ld", &num);
    size_t x = 5;
    size_t roundoff = (num + x - 1) / x;
    printf("Rounded off value: %ld\n", roundoff*x);

    return 0;
}
/* hello.c - Sample user program using libc */

#include "stdio.h"
#include "stdlib.h"

static void delay(void) {
    volatile int i;
    for (i = 0; i < 500000; i++);
}

int main(void) {
    printf("Hello from user mode!\n");
    delay();
    printf("Running in Ring 3 via libc\n");
    delay();
    exit(0);
    return 0;
}
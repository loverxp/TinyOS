/* stdlib.c - Standard library implementation for user programs */

#include "stdlib.h"
#include "syscall.h"

/* ── Bump allocator ── */
#define HEAP_SIZE (16 * 1024)  /* 16 KB heap */
static char heap[HEAP_SIZE];
static uint32_t heap_used = 0;

void* malloc(uint32_t size) {
    /* Align to 4 bytes */
    size = (size + 3) & ~3;
    if (heap_used + size > HEAP_SIZE)
        return 0;  /* Out of memory */
    void* ptr = &heap[heap_used];
    heap_used += size;
    return ptr;
}

void free(void* ptr) {
    (void)ptr;
    /* Bump allocator: free is a no-op */
}

void* calloc(uint32_t num, uint32_t size) {
    uint32_t total = num * size;
    void* ptr = malloc(total);
    if (ptr) {
        char* p = (char*)ptr;
        for (uint32_t i = 0; i < total; i++)
            p[i] = 0;
    }
    return ptr;
}

/* ── Exit ── */
void exit(int code) {
    (void)code;
    asm volatile("mov $0, %%eax; int $0x80" : : : "eax", "memory");
    for (;;);  /* Never reached */
}
#include "../include/mm.h"
#include "../include/stdio.h"
#include "../include/string.h"

// Kernel heap - placed in BSS section
#define HEAP_SIZE (64 * 1024)  // 64KB heap
static char heap[HEAP_SIZE];
static uint32_t heap_used = 0;

void* kmalloc(size_t size) {
    // Align to 4 bytes
    size = (size + 3) & ~3;
    
    if (heap_used + size > HEAP_SIZE) {
        printf("[MM] kmalloc failed: out of memory\n");
        return NULL;
    }
    
    void* ptr = &heap[heap_used];
    heap_used += size;
    
    return ptr;
}

void kfree(void* ptr) {
    // Bump allocator - free does nothing
    (void)ptr;
}

void mm_test(void) {
    printf("\n--- kmalloc Test ---\n");
    
    void* p1 = kmalloc(16);
    printf("kmalloc(16) = %p\n", p1);
    
    void* p2 = kmalloc(32);
    printf("kmalloc(32) = %p\n", p2);
    
    void* p3 = kmalloc(64);
    printf("kmalloc(64) = %p\n", p3);
    
    if (p1) {
        strcpy((char*)p1, "Hello");
        printf("String: %s\n", (char*)p1);
    }
    
    if (p2) {
        int* arr = (int*)p2;
        arr[0] = 10;
        arr[1] = 20;
        arr[2] = 30;
        printf("Array: %d, %d, %d\n", arr[0], arr[1], arr[2]);
    }
    
    printf("--- Done ---\n");
}

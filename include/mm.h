#ifndef MM_H
#define MM_H

#include "types.h"

// Initialize kernel heap allocator
void mm_init(void);

// Allocate memory from kernel heap
void* kmalloc(size_t size);

// Free memory back to kernel heap
void kfree(void* ptr);

// Get heap statistics
uint32_t kmalloc_get_used(void);
uint32_t kmalloc_get_free(void);
uint32_t kmalloc_get_total(void);

// Test memory allocation
void mm_test(void);

#endif // MM_H
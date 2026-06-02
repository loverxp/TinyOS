#ifndef MM_H
#define MM_H

#include "types.h"

// Simple kernel heap allocator
// NO paging, just a bump allocator from BSS

// Allocate memory from kernel heap
void* kmalloc(size_t size);

// Free memory (currently does nothing)
void kfree(void* ptr);

// Test memory allocation
void mm_test(void);

#endif // MM_H

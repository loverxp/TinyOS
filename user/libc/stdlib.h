/* stdlib.h - Standard library functions */

#ifndef STDLIB_H
#define STDLIB_H

#include <stdint.h>

/* Exit program with status code */
void exit(int code) __attribute__((noreturn));

/* Allocate memory - simple bump allocator */
void* malloc(uint32_t size);

/* Free memory (no-op with simple bump allocator) */
void free(void* ptr);

/* Allocate zeroed memory */
void* calloc(uint32_t num, uint32_t size);

#endif /* STDLIB_H */
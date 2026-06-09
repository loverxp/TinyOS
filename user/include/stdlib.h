/* stdlib.h - Standard library functions */

#ifndef STDLIB_H
#define STDLIB_H

#include <stddef.h>
#include <stdint.h>

/* Exit program with status code */
void exit(int code) __attribute__((noreturn));

/* Allocate memory - simple bump allocator */
void* malloc(size_t size);

/* Free memory (no-op with simple bump allocator) */
void free(void* ptr);

/* Allocate zeroed memory */
void* calloc(size_t num, size_t size);

/* Reallocate memory */
void* realloc(void* ptr, size_t size);

/* Convert string to integer */
int atoi(const char* s);

/* Convert string to long integer */
long strtol(const char* s, char** endptr, int base);

/* Convert string to unsigned long */
unsigned long strtoul(const char* s, char** endptr, int base);

/* Return absolute value */
int abs(int n);

#endif /* STDLIB_H */

#ifndef STRING_H
#define STRING_H

#include "types.h"

// Calculate string length
size_t strlen(const char* str);

// Copy string from src to dest
char* strcpy(char* dest, const char* src);

// Copy n characters from src to dest
char* strncpy(char* dest, const char* src, size_t n);

// Concatenate src to dest
char* strcat(char* dest, const char* src);

// Compare two strings
int strcmp(const char* s1, const char* s2);

// Compare n characters of two strings
int strncmp(const char* s1, const char* s2, size_t n);

// Find first occurrence of c in s
char* strchr(const char* s, int c);

// Fill memory with a byte value
void* memset(void* ptr, int value, size_t num);

// Copy memory from source to destination
void* memcpy(void* dest, const void* src, size_t num);

// Compare two memory areas
int memcmp(const void* s1, const void* s2, size_t n);

// Convert integer to string
void itoa(int value, char* str, int base);

// Convert unsigned integer to string
void uitoa(unsigned int value, char* str, int base);

#endif // STRING_H

/* string.h - String utility functions */

#ifndef STRING_H
#define STRING_H

#include <stdint.h>

uint32_t strlen(const char* s);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, uint32_t n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* s1, const char* s2, uint32_t n);
char* strcat(char* dest, const char* src);
char* strchr(const char* s, int c);

void* memset(void* ptr, int value, uint32_t num);
void* memcpy(void* dest, const void* src, uint32_t num);
int memcmp(const void* s1, const void* s2, uint32_t n);

#endif /* STRING_H */
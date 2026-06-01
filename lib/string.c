#include "../include/string.h"

// Calculate string length
size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

// Copy string from src to dest
char* strcpy(char* dest, const char* src) {
    char* original_dest = dest;
    while ((*dest++ = *src++) != '\0');
    return original_dest;
}

// Copy n characters from src to dest
char* strncpy(char* dest, const char* src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

// Concatenate src to dest
char* strcat(char* dest, const char* src) {
    char* original_dest = dest;
    while (*dest != '\0') {
        dest++;
    }
    while ((*dest++ = *src++) != '\0');
    return original_dest;
}

// Compare two strings
int strcmp(const char* s1, const char* s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// Compare n characters of two strings
int strncmp(const char* s1, const char* s2, size_t n) {
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0) {
        return 0;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

// Find first occurrence of c in s
char* strchr(const char* s, int c) {
    while (*s != '\0') {
        if (*s == (char)c) {
            return (char*)s;
        }
        s++;
    }
    return NULL;
}

// Fill memory with a byte value
void* memset(void* ptr, int value, size_t num) {
    unsigned char* p = ptr;
    while (num--) {
        *p++ = (unsigned char)value;
    }
    return ptr;
}

// Copy memory from source to destination
void* memcpy(void* dest, const void* src, size_t num) {
    unsigned char* d = dest;
    const unsigned char* s = src;
    while (num--) {
        *d++ = *s++;
    }
    return dest;
}

// Compare two memory areas
int memcmp(const void* s1, const void* s2, size_t n) {
    const unsigned char* p1 = s1;
    const unsigned char* p2 = s2;
    while (n--) {
        if (*p1 != *p2) {
            return *p1 - *p2;
        }
        p1++;
        p2++;
    }
    return 0;
}

// Convert integer to string
void itoa(int value, char* str, int base) {
    char* rc;
    char* ptr;
    char* low;
    
    // Check for supported base
    if (base < 2 || base > 36) {
        *str = '\0';
        return;
    }
    
    rc = ptr = str;
    
    // Set '-' for negative decimals
    if (value < 0 && base == 10) {
        *ptr++ = '-';
    }
    
    // Remember where the numbers start
    low = ptr;
    
    // The actual conversion
    do {
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + value % base];
        value /= base;
    } while (value);
    
    // Terminate the string
    *ptr-- = '\0';
    
    // Invert the numbers
    while (low < ptr) {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
}

// Convert unsigned integer to string
void uitoa(unsigned int value, char* str, int base) {
    char* ptr;
    char* low;
    
    if (base < 2 || base > 36) {
        *str = '\0';
        return;
    }
    
    ptr = str;
    
    low = ptr;
    
    do {
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + value % base];
        value /= base;
    } while (value);
    
    *ptr-- = '\0';
    
    while (low < ptr) {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
}

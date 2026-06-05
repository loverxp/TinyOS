#include "../include/stdio.h"
#include "../include/vga.h"
#include "../include/io.h"
#include "../include/string.h"
#include "../include/serial.h"

// Internal buffer for formatting
#define PRINTF_BUF_SIZE 1024

// Helper: convert unsigned int to string
static char* uitoa_str(unsigned int value, char* str, int base) {
    char* ptr = str;
    char* low = ptr;
    
    do {
        unsigned int digit = value % base;
        *ptr++ = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        value /= base;
    } while (value);
    
    *ptr-- = '\0';
    
    while (low < ptr) {
        char tmp = *low;
        *low++ = *ptr;
        *ptr-- = tmp;
    }
    
    return str;
}

// Helper: convert int to string
static char* itoa_str(int value, char* str, int base) {
    char* ptr = str;
    
    if (value < 0 && base == 10) {
        *ptr++ = '-';
        value = -value;
    }
    
    uitoa_str((unsigned int)value, ptr, base);
    return str;
}

// Internal vsprintf implementation
static int vsprintf_internal(char* buf, const char* fmt, __builtin_va_list args) {
    char* str = buf;
    char num_buf[32];
    
    while (*fmt) {
        if (*fmt != '%') {
            *str++ = *fmt++;
            continue;
        }
        
        fmt++; // skip '%'
        
        /* Parse optional flags and width */
        int pad_zero = 0;
        int left_align = 0;
        int width = 0;
        /* Parse flags */
        while (*fmt == '0' || *fmt == '-') {
            if (*fmt == '0') pad_zero = 1;
            if (*fmt == '-') left_align = 1;
            fmt++;
        }
        /* Parse width */
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        
        switch (*fmt) {
            case 'c': {
                char c = (char)__builtin_va_arg(args, int);
                *str++ = c;
                break;
            }
            case 's': {
                const char* s = __builtin_va_arg(args, const char*);
                if (!s) s = "(null)";
                int len = 0;
                for (const char* p = s; *p; p++) len++;
                if (!left_align) {
                    while (len < width) { *str++ = ' '; width--; }
                }
                int w = width;
                while (*s) *str++ = *s++;
                if (left_align) {
                    while (len < w) { *str++ = ' '; w--; len++; }
                }
                break;
            }
            case 'd':
            case 'i': {
                int i = __builtin_va_arg(args, int);
                itoa_str(i, num_buf, 10);
                int len = 0;
                for (const char* p = num_buf; *p; p++) len++;
                if (!left_align) {
                    int w = width;
                    while (len < w) { *str++ = ' '; w--; }
                }
                char* s = num_buf;
                while (*s) *str++ = *s++;
                if (left_align) {
                    int w = width;
                    while (len < w) { *str++ = ' '; w--; len++; }
                }
                break;
            }
            case 'u': {
                unsigned int u = __builtin_va_arg(args, unsigned int);
                uitoa_str(u, num_buf, 10);
                int len = 0;
                for (const char* p = num_buf; *p; p++) len++;
                if (!left_align) {
                    int w = width;
                    while (len < w) { *str++ = ' '; w--; }
                }
                char* s = num_buf;
                while (*s) *str++ = *s++;
                if (left_align) {
                    int w = width;
                    while (len < w) { *str++ = ' '; w--; len++; }
                }
                break;
            }
            case 'x': {
                unsigned int x = __builtin_va_arg(args, unsigned int);
                uitoa_str(x, num_buf, 16);
                int len = 0;
                for (const char* p = num_buf; *p; p++) len++;
                char pad_ch = pad_zero ? '0' : ' ';
                if (!left_align) {
                    int w = width;
                    while (len < w) { *str++ = pad_ch; w--; }
                }
                char* s = num_buf;
                while (*s) *str++ = *s++;
                if (left_align) {
                    int w = width;
                    while (len < w) { *str++ = ' '; w--; len++; }
                }
                break;
            }
            case 'X': {
                unsigned int X = __builtin_va_arg(args, unsigned int);
                uitoa_str(X, num_buf, 16);
                int len = 0;
                for (const char* p = num_buf; *p; p++) len++;
                char pad_ch = pad_zero ? '0' : ' ';
                if (!left_align) {
                    int w = width;
                    while (len < w) { *str++ = pad_ch; w--; }
                }
                char* s = num_buf;
                while (*s) {
                    if (*s >= 'a' && *s <= 'f') {
                        *str++ = *s++ - 'a' + 'A';
                    } else {
                        *str++ = *s++;
                    }
                }
                if (left_align) {
                    int w = width;
                    while (len < w) { *str++ = ' '; w--; len++; }
                }
                break;
            }
            case 'p': {
                void* p = __builtin_va_arg(args, void*);
                *str++ = '0';
                *str++ = 'x';
                uitoa_str((unsigned int)p, num_buf, 16);
                char* s = num_buf;
                while (*s) *str++ = *s++;
                break;
            }
            case '%':
                *str++ = '%';
                break;
            default:
                *str++ = '%';
                *str++ = *fmt;
                break;
        }
        fmt++;
    }
    
    *str = '\0';
    return str - buf;
}

// sprintf: format to string buffer
int sprintf(char* buf, const char* fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    int ret = vsprintf_internal(buf, fmt, args);
    __builtin_va_end(args);
    return ret;
}

// Print formatted string to VGA and serial
void printf(const char* fmt, ...) {
    char buf[PRINTF_BUF_SIZE];
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    vsprintf_internal(buf, fmt, args);
    __builtin_va_end(args);
    vga_writestring(buf);
    serial_writestring(buf);
}

// Print a single character
void putchar(char c) {
    vga_putchar(c);
    serial_putchar(c);
}

// Print a string with newline
void puts(const char* s) {
    vga_writestring(s);
    vga_putchar('\n');
    serial_writestring(s);
    serial_putchar('\n');
}

// Print formatted string to serial port (uses driver)
void serial_printf(const char* fmt, ...) {
    char buf[PRINTF_BUF_SIZE];
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    vsprintf_internal(buf, fmt, args);
    __builtin_va_end(args);
    serial_writestring(buf);
}

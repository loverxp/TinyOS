/* stdio.c - Standard I/O implementation for user programs */

#include "stdio.h"
#include "stdarg.h"
#include "syscall.h"

/* Forward declaration */
int vsprintf(char* buf, const char* fmt, va_list args);

/* Writes string to VGA via syscall 7 (console write). */
static void console_write(const char* s) {
    asm volatile("mov $7, %%eax; mov %0, %%ebx; int $0x80"
        : : "r"((uint32_t)s) : "eax", "ebx", "memory");
}

int puts(const char* s) {
    console_write(s);
    console_write("\n");
    return 0;
}

int putchar(int c) {
    char buf[2] = { (char)c, '\0' };
    console_write(buf);
    return c;
}

/* Simple printf implementation (integer/string only) */
int printf(const char* fmt, ...) {
    char buf[256];
    int ret;
    va_list args;
    va_start(args, fmt);
    ret = vsprintf(buf, fmt, args);
    va_end(args);
    console_write(buf);
    return ret;
}

/* Minimal sprintf - only handles %d, %x, %s, %c, %p */
int vsprintf(char* buf, const char* fmt, va_list args) {
    char* p = buf;
    while (*fmt) {
        if (*fmt != '%') { *p++ = *fmt++; continue; }
        fmt++;
        int zero_pad = 0, width = 0;
        if (*fmt == '0') { zero_pad = 1; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }
        switch (*fmt) {
            case 'd': {
                int v = va_arg(args, int);
                char tmp[32];
                int neg = 0, i = 0, wi;
                if (v < 0) { neg = 1; v = -v; }
                if (v == 0) tmp[i++] = '0';
                while (v > 0) { tmp[i++] = '0' + (v % 10); v /= 10; }
                wi = i + neg;
                while (wi < width) { *p++ = zero_pad ? '0' : ' '; wi++; }
                if (neg) *p++ = '-';
                while (i > 0) *p++ = tmp[--i];
                break;
            }
            case 'x': case 'X': {
                uint32_t v = va_arg(args, uint32_t);
                char tmp[16];
                int i = 0, wi;
                if (v == 0) tmp[i++] = '0';
                while (v > 0) {
                    int d = v & 0xF;
                    tmp[i++] = (*fmt == 'X' ? "0123456789ABCDEF" : "0123456789abcdef")[d];
                    v >>= 4;
                }
                wi = i;
                while (wi < width) { *p++ = zero_pad ? '0' : ' '; wi++; }
                while (i > 0) *p++ = tmp[--i];
                break;
            }
            case 's': {
                const char* s = va_arg(args, const char*);
                if (!s) s = "(null)";
                while (*s) *p++ = *s++;
                break;
            }
            case 'c': {
                int c = va_arg(args, int);
                *p++ = (char)c;
                break;
            }
            case 'p': {
                *p++ = '0'; *p++ = 'x';
                uint32_t v = va_arg(args, uint32_t);
                char tmp[16];
                int i = 0;
                if (v == 0) tmp[i++] = '0';
                while (v > 0) {
                    tmp[i++] = "0123456789abcdef"[v & 0xF];
                    v >>= 4;
                }
                while (i > 0) *p++ = tmp[--i];
                break;
            }
            default:
                *p++ = *fmt;
                break;
        }
        if (*fmt) fmt++;
    }
    *p = '\0';
    return p - buf;
}

int sprintf(char* buf, const char* fmt, ...) {
    int ret;
    va_list args;
    va_start(args, fmt);
    ret = vsprintf(buf, fmt, args);
    va_end(args);
    return ret;
}
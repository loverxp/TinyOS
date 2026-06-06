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
            case 'u': {
                unsigned int v = va_arg(args, unsigned int);
                char tmp[32];
                int i = 0, wi;
                if (v == 0) tmp[i++] = '0';
                while (v > 0) { tmp[i++] = '0' + (v % 10); v /= 10; }
                wi = i;
                while (wi < width) { *p++ = zero_pad ? '0' : ' '; wi++; }
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

/* ── scanf ────────────────────────────────────────────────────────── */

/* Skip whitespace in input (space, tab, newline) */
static void skip_whitespace(void) {
    char c;
    while (1) {
        c = getchar();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') continue;
        // Put back by making it available again? Can't unget with our simple setup.
        // Instead we just return the first non-whitespace char, but we can't push it back...
        // Solution: scanf operates on a line at a time (as most simple implementations do)
        break;
    }
}

/* Simple scanf implementation.
 * Reads one line via readline(), then parses it.
 * Supports: %d %u %x %s %c
 */
int scanf(const char* fmt, ...) {
    char line[256];
    int len = readline(line, sizeof(line));
    if (len <= 0) return 0;

    va_list args;
    va_start(args, fmt);

    const char* p = line;
    int items = 0;

    while (*fmt) {
        // Skip whitespace in format
        if (*fmt == ' ' || *fmt == '\t' || *fmt == '\n') {
            fmt++;
            continue;
        }

        if (*fmt != '%') {
            // Literal character match
            // Skip whitespace in input if format has space
            if (*fmt == ' ') {
                while (*p == ' ' || *p == '\t') p++;
            } else if (*p == *fmt) {
                p++;
            } else {
                break; // mismatch
            }
            fmt++;
            continue;
        }

        fmt++; // skip '%'
        int is_long = 0;
        if (*fmt == 'l') { is_long = 1; fmt++; }

        switch (*fmt) {
            case 'd': {
                // Skip whitespace
                while (*p == ' ' || *p == '\t') p++;
                int neg = 0;
                if (*p == '-') { neg = 1; p++; }
                int val = 0;
                while (*p >= '0' && *p <= '9') {
                    val = val * 10 + (*p - '0');
                    p++;
                }
                if (neg) val = -val;
                if (is_long) {
                    long* out = va_arg(args, long*);
                    *out = val;
                } else {
                    int* out = va_arg(args, int*);
                    *out = val;
                }
                items++;
                break;
            }
            case 'u': {
                while (*p == ' ' || *p == '\t') p++;
                unsigned int val = 0;
                while (*p >= '0' && *p <= '9') {
                    val = val * 10 + (*p - '0');
                    p++;
                }
                unsigned int* out = va_arg(args, unsigned int*);
                *out = val;
                items++;
                break;
            }
            case 'x': case 'X': {
                while (*p == ' ' || *p == '\t') p++;
                if (*p == '0' && (*(p+1) == 'x' || *(p+1) == 'X')) p += 2;
                unsigned int val = 0;
                while (1) {
                    char c = *p;
                    if (c >= '0' && c <= '9') { val = val * 16 + (c - '0'); p++; }
                    else if (c >= 'a' && c <= 'f') { val = val * 16 + (c - 'a' + 10); p++; }
                    else if (c >= 'A' && c <= 'F') { val = val * 16 + (c - 'A' + 10); p++; }
                    else break;
                }
                unsigned int* out = va_arg(args, unsigned int*);
                *out = val;
                items++;
                break;
            }
            case 's': {
                while (*p == ' ' || *p == '\t') p++;
                char* out = va_arg(args, char*);
                while (*p && *p != ' ' && *p != '\t' && *p != '\n') {
                    *out++ = *p++;
                }
                *out = '\0';
                items++;
                break;
            }
            case 'c': {
                char* out = va_arg(args, char*);
                if (*p) {
                    *out = *p++;
                    items++;
                }
                break;
            }
            default:
                fmt++;
                break;
        }
        fmt++;
    }

    va_end(args);
    return items;
}
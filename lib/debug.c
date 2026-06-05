#include "../include/debug.h"
#include "../include/serial.h"
#include "../include/stdio.h"
#include "../include/string.h"

static const char* level_names[] = { "ERR", "WRN", "INF", "DBG" };

void kprintf(int level, const char* module, const char* fmt, ...) {
    if (level > KLOG_LEVEL) return;

    char buf[256];
    char prefix[48];
    sprintf(prefix, "[%s][%s] ", level_names[level], module);

    /* Write prefix */
    serial_writestring(prefix);

    /* Format user message */
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    /* Manual format into buf (reuse vsprintf pattern) */
    /* We call sprintf for simplicity — reformat with args */
    /* Since we can't pass va_list to sprintf easily without vsprintf,
       we use a direct approach: */
    char* str = buf;
    const char* f = fmt;
    char num_buf[32];

    while (*f) {
        if (*f != '%') {
            *str++ = *f++;
            continue;
        }
        f++;

        int pad_zero = 0;
        int width = 0;
        while (*f == '0') { pad_zero = 1; f++; }
        while (*f >= '0' && *f <= '9') { width = width * 10 + (*f - '0'); f++; }

        switch (*f) {
        case 'd': case 'i': {
            int v = __builtin_va_arg(args, int);
            char* p = num_buf;
            if (v < 0) { *str++ = '-'; v = -v; }
            char* q = p;
            unsigned int u = (unsigned int)v;
            do { *q++ = '0' + u % 10; u /= 10; } while (u);
            int len = q - p;
            char ch = pad_zero ? '0' : ' ';
            while (len < width) { *str++ = ch; width--; }
            while (q > p) *str++ = *--q;
            break;
        }
        case 'u': {
            unsigned int u = __builtin_va_arg(args, unsigned int);
            char* p = num_buf; char* q = p;
            do { *q++ = '0' + u % 10; u /= 10; } while (u);
            int len = q - p;
            char ch = pad_zero ? '0' : ' ';
            while (len < width) { *str++ = ch; width--; }
            while (q > p) *str++ = *--q;
            break;
        }
        case 'x': {
            unsigned int x = __builtin_va_arg(args, unsigned int);
            const char* hex = "0123456789abcdef";
            char* p = num_buf; char* q = p;
            do { *q++ = hex[x & 0xF]; x >>= 4; } while (x);
            int len = q - p;
            char ch = pad_zero ? '0' : ' ';
            while (len < width) { *str++ = ch; width--; }
            while (q > p) *str++ = *--q;
            break;
        }
        case 'X': {
            unsigned int x = __builtin_va_arg(args, unsigned int);
            const char* hex = "0123456789ABCDEF";
            char* p = num_buf; char* q = p;
            do { *q++ = hex[x & 0xF]; x >>= 4; } while (x);
            int len = q - p;
            char ch = pad_zero ? '0' : ' ';
            while (len < width) { *str++ = ch; width--; }
            while (q > p) *str++ = *--q;
            break;
        }
        case 's': {
            const char* s = __builtin_va_arg(args, const char*);
            if (!s) s = "(null)";
            while (*s) *str++ = *s++;
            break;
        }
        case 'c':
            *str++ = (char)__builtin_va_arg(args, int);
            break;
        case 'p': {
            void* p = __builtin_va_arg(args, void*);
            *str++ = '0'; *str++ = 'x';
            const char* hex = "0123456789abcdef";
            unsigned int pv = (unsigned int)p;
            char* q = num_buf; char* t = q;
            do { *t++ = hex[pv & 0xF]; pv >>= 4; } while (pv);
            while (t > q) *str++ = *--t;
            break;
        }
        case '%': *str++ = '%'; break;
        default: *str++ = '%'; *str++ = *f; break;
        }
        f++;
    }
    *str = '\0';
    __builtin_va_end(args);

    serial_writestring(buf);
    serial_putchar('\n');
}

/* Walk EBP chain and print return addresses to serial */
void kernel_backtrace(void) {
    uint32_t* ebp;
    asm volatile("mov %%ebp, %0" : "=r"(ebp));

    serial_writestring("[Backtrace]\n");
    for (int i = 0; i < 10 && ebp; i++) {
        /* ebp[0] = previous ebp, ebp[1] = return address */
        uint32_t ret_addr = ebp[1];
        if (ret_addr == 0) break;
        char buf[64];
        sprintf(buf, "  #%d: 0x%x\n", i, ret_addr);
        serial_writestring(buf);
        ebp = (uint32_t*)ebp[0];
    }
}

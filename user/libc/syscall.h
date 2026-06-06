/* syscall.h - System call wrappers for user programs */

#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>

/* Non-blocking read key - returns 0 if no key pressed */
static inline uint32_t read_key(void) {
    uint32_t result;
    asm volatile("mov $3, %%eax; int $0x80" : "=a"(result) : : "ebx", "ecx", "memory");
    return result;
}

/* Get timer ticks */
static inline uint32_t get_ticks(void) {
    uint32_t result;
    asm volatile("mov $2, %%eax; int $0x80" : "=a"(result) : : "ebx", "ecx", "memory");
    return result;
}

/* Set video mode: 0=text(03h), 1=graphics(13h) */
static inline void set_video_mode(int mode) {
    asm volatile("mov $4, %%eax; mov %0, %%ebx; int $0x80"
        : : "r"((uint32_t)mode) : "eax", "ebx", "memory");
}

/* Clear keyboard buffer */
static inline void clear_keybuf(void) {
    asm volatile("mov $5, %%eax; int $0x80" : : : "eax", "memory");
}

/* Debug print to serial port */
static inline void debug_print(const char* msg) {
    asm volatile("mov $6, %%eax; mov %0, %%ebx; int $0x80"
        : : "r"((uint32_t)msg) : "eax", "ebx", "memory");
}

/* Blocking read a character from keyboard */
static inline char getchar(void) {
    uint32_t result;
    asm volatile("mov $21, %%eax; int $0x80"
        : "=a"(result) : : "ebx", "ecx", "memory");
    return (char)result;
}

/* Blocking read a line of input */
static inline int readline(char* buf, int max) {
    uint32_t result;
    asm volatile("mov $22, %%eax; mov %1, %%ebx; mov %2, %%ecx; int $0x80"
        : "=a"(result) : "r"((uint32_t)buf), "r"((uint32_t)max)
        : "ebx", "ecx", "memory");
    return (int)result;
}

/* Get command line arguments for current user command */
static inline int get_cmdline(char* buf, int max) {
    uint32_t result;
    asm volatile("mov $23, %%eax; mov %1, %%ebx; mov %2, %%ecx; int $0x80"
        : "=a"(result) : "r"((uint32_t)buf), "r"((uint32_t)max)
        : "ebx", "ecx", "memory");
    return (int)result;
}

/* Clear screen (VGA text mode) */
static inline void clear_screen(void) {
    asm volatile("mov $24, %%eax; int $0x80" : : : "eax", "memory");
}

#endif /* SYSCALL_H */
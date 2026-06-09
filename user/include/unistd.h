/* unistd.h - POSIX-like system call wrappers */

#ifndef UNISTD_H
#define UNISTD_H

#include <stdint.h>
#include <stddef.h>

/* File access modes */
#define R_OK 4
#define W_OK 2
#define X_OK 1
#define F_OK 0

/* Seek origins */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* File descriptor I/O */
static inline int read(int fd, void* buf, uint32_t size) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov %3, %%edx; mov $29, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)fd), "r"((uint32_t)buf), "r"(size)
        : "ebx", "ecx", "edx", "memory");
    return (int)result;
}

static inline int write(int fd, const void* buf, uint32_t size) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov %3, %%edx; mov $30, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)fd), "r"((uint32_t)buf), "r"(size)
        : "ebx", "ecx", "edx", "memory");
    return (int)result;
}

static inline int close(int fd) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov $31, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)fd) : "ebx", "memory");
    return (int)result;
}

/* Process management */
static inline int fork(void) {
    uint32_t result;
    asm volatile("mov $25, %%eax; int $0x80"
        : "=a"(result) : : "ebx", "ecx", "memory");
    return (int)result;
}

static inline void _exit(int code) __attribute__((noreturn));
static inline void _exit(int code) {
    asm volatile("mov %0, %%ebx; mov $0, %%eax; int $0x80" : : "r"(code) : "eax", "ebx", "memory");
    for (;;);
}

static inline int getpid(void) {
    uint32_t result;
    asm volatile("mov $56, %%eax; int $0x80"
        : "=a"(result) : : "ebx", "ecx", "memory");
    return (int)result;
}

/* Scheduling */
static inline void yield(void) {
    asm volatile("mov $8, %%eax; int $0x80" : : : "eax", "memory");
}

static inline void sleep_ms(uint32_t ms) {
    asm volatile("mov $9, %%eax; mov %0, %%ebx; int $0x80"
        : : "r"(ms) : "eax", "ebx", "memory");
}

/* File operations */
static inline int unlink(const char* path) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov $38, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)path) : "ebx", "memory");
    return (int)result;
}

static inline int chdir(const char* path) {
    (void)path;
    return -1; /* Not implemented yet */
}

/* sbrk: extend/shrink program break */
static inline void* sbrk(int increment) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov $59, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)increment) : "ebx", "memory");
    return (void*)result;
}

/* lseek: reposition read/write file offset */
static inline int lseek(int fd, int offset, int whence) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov %3, %%edx; mov $60, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)fd), "r"((uint32_t)offset), "r"((uint32_t)whence)
        : "ebx", "ecx", "edx", "memory");
    return (int)result;
}

#endif /* UNISTD_H */

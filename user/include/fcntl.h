/* fcntl.h - File control options, types, and open() wrapper */

#ifndef FCNTL_H
#define FCNTL_H

#include <stdint.h>

/* File open flags */
#define O_RDONLY  0x01
#define O_WRONLY  0x02
#define O_RDWR    0x03
#define O_CREATE  0x10
#define O_TRUNC   0x20
#define O_APPEND  0x40

/* File types */
#define FT_FILE   1
#define FT_DIR    2

/* VFS stat structure (matches kernel vfs_stat_t) */
typedef struct {
    uint32_t size;
    uint8_t  type;
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint16_t hour;
    uint8_t  minute;
} vfs_stat_t;

/* VFS directory entry (matches kernel vfs_dirent_t) */
typedef struct {
    char     name[14];
    uint8_t  type;
    uint32_t size;
} vfs_dirent_t;

/* open() syscall wrapper */
static inline int open(const char* path, int flags) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov $28, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)path), "r"((uint32_t)flags)
        : "ebx", "ecx", "memory");
    return (int)result;
}

/* mkdir() syscall wrapper */
static inline int mkdir(const char* path) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov $36, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)path) : "ebx", "memory");
    return (int)result;
}

/* rmdir() syscall wrapper */
static inline int rmdir(const char* path) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov $37, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)path) : "ebx", "memory");
    return (int)result;
}

/* fstat: get file info by file descriptor */
static inline int fstat(int fd, vfs_stat_t* buf) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov $61, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)fd), "r"((uint32_t)buf)
        : "ebx", "ecx", "memory");
    return (int)result;
}

#endif /* FCNTL_H */

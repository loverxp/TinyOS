#ifndef VFS_H
#define VFS_H

#include "types.h"

#define VFS_MAX_FDS       16
#define VFS_MAX_BACKENDS   4
#define VFS_PATH_MAX      128

#define VFS_O_RDONLY   0x01
#define VFS_O_WRONLY   0x02
#define VFS_O_RDWR     0x03
#define VFS_O_CREATE   0x10
#define VFS_O_TRUNC    0x20
#define VFS_O_APPEND   0x40

/* Seek origins */
#define VFS_SEEK_SET 0
#define VFS_SEEK_CUR 1
#define VFS_SEEK_END 2

#define VFS_TYPE_FILE  1
#define VFS_TYPE_DIR   2

typedef struct {
    uint32_t size;
    uint8_t  type;
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint16_t hour;
    uint8_t  minute;
} vfs_stat_t;

typedef struct {
    char     name[14];
    uint8_t  type;
    uint32_t size;
} vfs_dirent_t;

typedef struct {
    const char* name;
    int  (*open)(const char* path, int flags);
    int  (*read)(int handle, void* buf, uint32_t size);
    int  (*write)(int handle, const void* buf, uint32_t size);
    void (*close)(int handle);
    int  (*stat)(const char* path, vfs_stat_t* st);
    int  (*readdir)(int handle, vfs_dirent_t* dirent, uint32_t index);
    int  (*unlink)(const char* path);
    int  (*mkdir)(const char* path);
    int  (*rmdir)(const char* path);
    int  (*lseek)(int handle, int offset, int whence);
    int  (*fstat)(int handle, vfs_stat_t* st);
} vfs_backend_ops_t;

typedef struct {
    int      in_use;
    int      backend_id;
    int      handle;
    int      flags;
    uint32_t offset;
    char     path[VFS_PATH_MAX];
} vfs_fd_t;

void vfs_init(void);
int  vfs_register_backend(const vfs_backend_ops_t* ops);
int  vfs_open(const char* path, int flags);
int  vfs_read(int fd, void* buf, uint32_t size);
int  vfs_write(int fd, const void* buf, uint32_t size);
void vfs_close(int fd);
int  vfs_stat(const char* path, vfs_stat_t* st);
int  vfs_readdir(int fd, vfs_dirent_t* dirent, uint32_t index);
int  vfs_unlink(const char* path);
int  vfs_mkdir(const char* path);
int  vfs_rmdir(const char* path);
int  vfs_lseek(int fd, int offset, int whence);
int  vfs_fstat(int fd, vfs_stat_t* st);
int  vfs_dup2(int oldfd, int newfd);

#endif /* VFS_H */

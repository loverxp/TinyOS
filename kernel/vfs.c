#include "../include/vfs.h"
#include "../include/string.h"
#include "../include/stdio.h"

static vfs_fd_t fd_table[VFS_MAX_FDS];
static const vfs_backend_ops_t* backends[VFS_MAX_BACKENDS];
static int backend_count = 0;

/* ── DevFS: /dev/null and /dev/zero ──────────────────────────────────── */

#define DEV_NULL  0
#define DEV_ZERO  1

static int devfs_open(const char* path, int flags) {
    (void)flags;
    if (strcmp(path, "/dev/null") == 0) return DEV_NULL;
    if (strcmp(path, "/dev/zero") == 0) return DEV_ZERO;
    return -1;
}

static int devfs_read(int handle, void* buf, uint32_t size) {
    if (handle == DEV_NULL) return 0;        /* EOF */
    if (handle == DEV_ZERO) {
        memset(buf, 0, size);
        return (int)size;
    }
    return -1;
}

static int devfs_write(int handle, const void* buf, uint32_t size) {
    (void)buf;
    if (handle == DEV_NULL || handle == DEV_ZERO) return (int)size; /* discard */
    return -1;
}

static void devfs_close(int handle) {
    (void)handle;  /* nothing to clean up */
}

static int devfs_stat(const char* path, vfs_stat_t* st) {
    if (strcmp(path, "/dev/null") == 0 || strcmp(path, "/dev/zero") == 0) {
        memset(st, 0, sizeof(*st));
        st->size = 0;
        st->type = VFS_TYPE_FILE;
        return 0;
    }
    return -1;
}

const vfs_backend_ops_t devfs_ops = {
    .name    = "devfs",
    .open    = devfs_open,
    .read    = devfs_read,
    .write   = devfs_write,
    .close   = devfs_close,
    .stat    = devfs_stat,
    .readdir = NULL,
    .unlink  = NULL,
    .mkdir   = NULL,
    .rmdir   = NULL,
    .lseek   = NULL,
    .fstat   = NULL,
};

/* ── End DevFS ───────────────────────────────────────────────────────── */

void vfs_init(void) {
    memset(fd_table, 0, sizeof(fd_table));
    memset(backends, 0, sizeof(backends));
    backend_count = 0;
}

int vfs_register_backend(const vfs_backend_ops_t* ops) {
    if (backend_count >= VFS_MAX_BACKENDS) return -1;
    backends[backend_count] = ops;
    serial_printf("[VFS] Registered backend: %s\n", ops->name);
    return backend_count++;
}

static int alloc_fd(void) {
    for (int i = 0; i < VFS_MAX_FDS; i++) {
        if (!fd_table[i].in_use) return i;
    }
    return -1;
}

int vfs_open(const char* path, int flags) {
    if (backend_count == 0) return -1;

    int fd = alloc_fd();
    if (fd < 0) return -1;

    /* Try each backend until one accepts the path */
    for (int b = 0; b < backend_count; b++) {
        int handle = backends[b]->open(path, flags);
        if (handle >= 0) {
            fd_table[fd].in_use = 1;
            fd_table[fd].backend_id = b;
            fd_table[fd].handle = handle;
            fd_table[fd].flags = flags;
            fd_table[fd].offset = 0;
            int i;
            for (i = 0; i < VFS_PATH_MAX - 1 && path[i]; i++)
                fd_table[fd].path[i] = path[i];
            fd_table[fd].path[i] = '\0';
            return fd;
        }
    }
    return -1;
}

int vfs_read(int fd, void* buf, uint32_t size) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !fd_table[fd].in_use) return -1;
    int n = backends[fd_table[fd].backend_id]->read(fd_table[fd].handle, buf, size);
    if (n > 0) fd_table[fd].offset += n;
    return n;
}

int vfs_write(int fd, const void* buf, uint32_t size) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !fd_table[fd].in_use) return -1;
    int n = backends[fd_table[fd].backend_id]->write(fd_table[fd].handle, buf, size);
    if (n > 0) fd_table[fd].offset += n;
    return n;
}

void vfs_close(int fd) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !fd_table[fd].in_use) return;
    backends[fd_table[fd].backend_id]->close(fd_table[fd].handle);
    memset(&fd_table[fd], 0, sizeof(vfs_fd_t));
}

int vfs_stat(const char* path, vfs_stat_t* st) {
    if (backend_count == 0) return -1;
    for (int b = 0; b < backend_count; b++) {
        if (backends[b]->stat && backends[b]->stat(path, st) == 0) return 0;
    }
    return -1;
}

int vfs_readdir(int fd, vfs_dirent_t* dirent, uint32_t index) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !fd_table[fd].in_use) return -1;
    return backends[fd_table[fd].backend_id]->readdir(fd_table[fd].handle, dirent, index);
}

int vfs_unlink(const char* path) {
    if (backend_count == 0) return -1;
    if (backends[0]->unlink) return backends[0]->unlink(path);
    return -1;
}

int vfs_mkdir(const char* path) {
    if (backend_count == 0) return -1;
    if (backends[0]->mkdir) return backends[0]->mkdir(path);
    return -1;
}

int vfs_rmdir(const char* path) {
    if (backend_count == 0) return -1;
    if (backends[0]->rmdir) return backends[0]->rmdir(path);
    return -1;
}

int vfs_lseek(int fd, int offset, int whence) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !fd_table[fd].in_use) return -1;
    if (!backends[fd_table[fd].backend_id]->lseek) return -1;
    int result = backends[fd_table[fd].backend_id]->lseek(fd_table[fd].handle, offset, whence);
    if (result >= 0) fd_table[fd].offset = (uint32_t)result;
    return result;
}

int vfs_fstat(int fd, vfs_stat_t* st) {
    if (fd < 0 || fd >= VFS_MAX_FDS || !fd_table[fd].in_use) return -1;
    if (!backends[fd_table[fd].backend_id]->fstat) return -1;
    return backends[fd_table[fd].backend_id]->fstat(fd_table[fd].handle, st);
}

int vfs_dup2(int oldfd, int newfd) {
    /* Validate oldfd */
    if (oldfd < 0 || oldfd >= VFS_MAX_FDS || !fd_table[oldfd].in_use)
        return -1;
    /* Validate newfd range */
    if (newfd < 0 || newfd >= VFS_MAX_FDS)
        return -1;
    /* If same fd, return as-is (POSIX) */
    if (oldfd == newfd)
        return newfd;
    /* If newfd is open, close it first */
    if (fd_table[newfd].in_use)
        vfs_close(newfd);
    /* Re-open the same path to get an independent backend handle */
    int handle = backends[fd_table[oldfd].backend_id]->open(
        fd_table[oldfd].path, fd_table[oldfd].flags);
    if (handle < 0) return -1;
    fd_table[newfd].in_use = 1;
    fd_table[newfd].backend_id = fd_table[oldfd].backend_id;
    fd_table[newfd].handle = handle;
    fd_table[newfd].flags = fd_table[oldfd].flags;
    fd_table[newfd].offset = 0;
    int i;
    for (i = 0; i < VFS_PATH_MAX - 1 && fd_table[oldfd].path[i]; i++)
        fd_table[newfd].path[i] = fd_table[oldfd].path[i];
    fd_table[newfd].path[i] = '\0';
    return newfd;
}

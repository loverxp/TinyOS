/* syscall.h - TinyOS-specific system call wrappers
 *
 * Standard POSIX-like wrappers (read, write, open, close, fork, etc.)
 * are in <unistd.h> and <fcntl.h>. This file contains TinyOS-specific
 * syscalls that don't have POSIX equivalents.
 */

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

/* Replace the current user program.
 * entry = new EIP, user_esp = new user stack top.
 * Does NOT return on success.
 */
static inline void exec(uint32_t entry, uint32_t user_esp) {
    asm volatile("mov $26, %%eax; mov %0, %%ebx; mov %1, %%ecx; int $0x80"
        : : "r"(entry), "r"(user_esp) : "eax", "ebx", "ecx", "memory");
}

/* Get system information.
 * type: 0=uptime(ticks), 1=date(rtc_time_t), 2=rand(uint32_t),
 *       3=meminfo(4*uint32_t), 4=diskinfo(5*uint32_t),
 *       5=pci_info, 6=net_config, 7=net_stats, 8=arp_table
 * Returns bytes written to buffer.
 */
static inline int get_system_info(uint32_t type, void* buf, uint32_t max_len) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov %3, %%edx; mov $27, %%eax; int $0x80"
        : "=a"(result) : "r"(type), "r"((uint32_t)buf), "r"(max_len)
        : "ebx", "ecx", "edx", "memory");
    return (int)result;
}

/* stat() syscall wrapper */
static inline int stat(const char* path, void* buf) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov $32, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)path), "r"((uint32_t)buf)
        : "ebx", "ecx", "memory");
    return (int)result;
}

/* readdir() syscall wrapper */
static inline int readdir(int fd, void* dirent, uint32_t index) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov %3, %%edx; mov $33, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)fd), "r"((uint32_t)dirent), "r"(index)
        : "ebx", "ecx", "edx", "memory");
    return (int)result;
}

/* Signal constants */
#define SIGHUP    1
#define SIGINT    2
#define SIGKILL   9
#define SIGUSR1   10
#define SIGUSR2   12
#define SIGTERM   15

#define SIG_DFL   0
#define SIG_IGN   1

static inline int kill(int pid, int sig) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov $34, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)pid), "r"((uint32_t)sig)
        : "ebx", "ecx", "memory");
    return (int)result;
}

static inline int signal_set(int sig, uint32_t action) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov $35, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)sig), "r"(action)
        : "ebx", "ecx", "memory");
    return (int)result;
}

/* IPC syscalls */
static inline int pipe_create(void) {
    uint32_t result;
    asm volatile("mov $10, %%eax; int $0x80" : "=a"(result) : : "ebx", "ecx", "memory");
    return (int)result;
}

static inline int pipe_read(int id, void* buf, uint32_t size) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov %3, %%edx; mov $11, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)id), "r"((uint32_t)buf), "r"(size)
        : "ebx", "ecx", "edx", "memory");
    return (int)result;
}

static inline int pipe_write(int id, const void* buf, uint32_t size) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov %3, %%edx; mov $12, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)id), "r"((uint32_t)buf), "r"(size)
        : "ebx", "ecx", "edx", "memory");
    return (int)result;
}

static inline int pipe_close(int id) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov $13, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)id) : "ebx", "memory");
    return (int)result;
}

static inline int mq_create(void) {
    uint32_t result;
    asm volatile("mov $14, %%eax; int $0x80" : "=a"(result) : : "ebx", "ecx", "memory");
    return (int)result;
}

static inline int mq_send(int id, const void* msg, uint32_t len) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov %3, %%edx; mov $15, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)id), "r"((uint32_t)msg), "r"(len)
        : "ebx", "ecx", "edx", "memory");
    return (int)result;
}

static inline int mq_recv(int id, void* buf, uint32_t size) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov %3, %%edx; mov $16, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)id), "r"((uint32_t)buf), "r"(size)
        : "ebx", "ecx", "edx", "memory");
    return (int)result;
}

static inline int mq_close(int id) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov $17, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)id) : "ebx", "memory");
    return (int)result;
}

static inline void* shm_create(const char* name, uint32_t size) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov $18, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)name), "r"(size)
        : "ebx", "ecx", "memory");
    return (void*)result;
}

static inline void* shm_open(const char* name) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov $19, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)name) : "ebx", "memory");
    return (void*)result;
}

static inline int shm_close(const char* name) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov $20, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)name) : "ebx", "memory");
    return (int)result;
}

/* --- Socket syscalls (39-46) --- */

/* Simple network address: {ip (4 bytes), port (2 bytes)} */
typedef struct { uint32_t ip; uint16_t port; } net_addr_t;

#define SOCK_TYPE_TCP 1
#define SOCK_TYPE_UDP 2

static inline int socket_create(int type) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov $39, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)type) : "ebx", "memory");
    return (int)result;
}

static inline int socket_bind(int fd, uint16_t port) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov $40, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)fd), "r"((uint32_t)port)
        : "ebx", "ecx", "memory");
    return (int)result;
}

static inline int socket_connect(int fd, const net_addr_t* addr) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov $41, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)fd), "r"((uint32_t)addr)
        : "ebx", "ecx", "memory");
    return (int)result;
}

static inline int socket_send(int fd, const void* data, uint16_t len) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov %3, %%edx; mov $42, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)fd), "r"((uint32_t)data), "r"((uint32_t)len)
        : "ebx", "ecx", "edx", "memory");
    return (int)result;
}

static inline int socket_recv(int fd, void* buf, uint16_t max_len) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov %3, %%edx; mov $43, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)fd), "r"((uint32_t)buf), "r"((uint32_t)max_len)
        : "ebx", "ecx", "edx", "memory");
    return (int)result;
}

static inline int socket_listen(int fd) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov $44, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)fd) : "ebx", "memory");
    return (int)result;
}

static inline int socket_accept(int fd, net_addr_t* addr) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov $45, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)fd), "r"((uint32_t)addr)
        : "ebx", "ecx", "memory");
    return (int)result;
}

static inline int socket_close(int fd) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov $46, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)fd) : "ebx", "memory");
    return (int)result;
}

/* --- Network control syscalls (51-55) --- */

static inline int net_dns_resolve(const char* hostname, uint32_t* out_ip) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov $51, %%eax; int $0x80"
        : "=a"(result) : "r"((uint32_t)hostname), "r"((uint32_t)out_ip)
        : "ebx", "ecx", "memory");
    return (int)result;
}

static inline int net_ping(uint32_t ip, uint16_t id, uint16_t seq) {
    uint32_t result;
    asm volatile("mov %1, %%ebx; mov %2, %%ecx; mov %3, %%edx; mov $52, %%eax; int $0x80"
        : "=a"(result) : "r"(ip), "r"((uint32_t)id), "r"((uint32_t)seq)
        : "ebx", "ecx", "edx", "memory");
    return (int)result;
}

static inline int net_dhcp(void) {
    uint32_t result;
    asm volatile("mov $53, %%eax; int $0x80" : "=a"(result) : : "ebx", "ecx", "memory");
    return (int)result;
}

static inline int net_arp_flush(void) {
    uint32_t result;
    asm volatile("mov $54, %%eax; int $0x80" : "=a"(result) : : "ebx", "ecx", "memory");
    return (int)result;
}

static inline int net_reset_stats(void) {
    uint32_t result;
    asm volatile("mov $55, %%eax; int $0x80" : "=a"(result) : : "ebx", "ecx", "memory");
    return (int)result;
}

/* Non-blocking read character from keyboard (bypasses stdin_pipe).
 * Returns ASCII character, or 0 if no key pressed.
 * Useful for serial/pipe mode where read_key() PS/2 scancodes aren't available.
 */
static inline int read_char_nonblock(void) {
    uint32_t result;
    asm volatile("mov $58, %%eax; int $0x80" : "=a"(result) : : "ebx", "ecx", "memory");
    return (int)result;
}

#endif /* SYSCALL_H */

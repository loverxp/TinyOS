/* filetest.c - Test VFS file I/O syscalls from user space */
#include <syscall.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

typedef struct {
    uint32_t size;
    uint8_t  type;
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint16_t hour;
    uint8_t  minute;
} user_stat_t;

int main(void) {
    printf("=== File I/O Test ===\n\n");

    /* Test 1: stat existing file */
    printf("[Test 1] stat readme.txt\n");
    user_stat_t st;
    if (stat("readme.txt", &st) == 0) {
        printf("  PASS: size=%u type=%u date=%u-%02u-%02u %02u:%02u\n",
               st.size, st.type, st.year, st.month, st.day, st.hour, st.minute);
    } else {
        printf("  FAIL: stat returned error\n");
    }

    /* Test 2: open and read existing file */
    printf("[Test 2] open + read readme.txt\n");
    int fd = open("readme.txt", O_RDONLY);
    if (fd >= 0) {
        char buf[128];
        memset(buf, 0, sizeof(buf));
        int n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("  PASS: read %d bytes: %.60s\n", n, buf);
        } else {
            printf("  FAIL: read returned %d\n", n);
        }
        close(fd);
    } else {
        printf("  FAIL: open returned %d\n", fd);
    }

    /* Test 3: create and write a new file */
    printf("[Test 3] create + write vfsdat.txt\n");
    fd = open("vfsdat.txt", O_WRONLY | O_CREATE);
    if (fd >= 0) {
        const char* msg = "Hello from VFS user I/O!";
        int len = 0;
        while (msg[len]) len++;
        int n = write(fd, msg, len);
        close(fd);
        printf("  PASS: wrote %d bytes\n", n);
    } else {
        printf("  FAIL: open for write returned %d\n", fd);
    }

    /* Test 4: verify by reading back */
    printf("[Test 4] read back vfsdat.txt\n");
    fd = open("vfsdat.txt", O_RDONLY);
    if (fd >= 0) {
        char buf[64];
        memset(buf, 0, sizeof(buf));
        int n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("  PASS: content = '%s'\n", buf);
        } else {
            printf("  FAIL: read returned %d\n", n);
        }
        close(fd);
    } else {
        printf("  FAIL: open returned %d\n", fd);
    }

    printf("\n=== File I/O Test Done ===\n");
    return 0;
}

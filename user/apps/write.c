/* write.c - Write text to file (user-mode) */
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    char args[256];
    get_cmdline(args, sizeof(args));

    if (args[0] == '\0') {
        printf("Usage: write <file> <text>\n");
        return 1;
    }

    /* Extract filename (first word) */
    const char* p = args;
    const char* fname = p;
    while (*p && *p != ' ') p++;
    uint32_t fname_len = (uint32_t)(p - fname);
    if (fname_len == 0 || fname_len > 12) {
        printf("Invalid filename\n");
        return 1;
    }
    char fname_buf[13];
    memcpy(fname_buf, fname, fname_len);
    fname_buf[fname_len] = '\0';

    /* Skip to text */
    while (*p == ' ') p++;

    if (*p == '\0') {
        printf("Usage: write <file> <text>\n");
        return 1;
    }

    uint32_t text_len = strlen(p);
    int fd = open(fname_buf, O_WRONLY | O_CREATE | O_TRUNC);
    if (fd < 0) {
        printf("Failed to create file: %s\n", fname_buf);
        return 1;
    }

    int written = write(fd, p, text_len);
    close(fd);

    if (written >= 0) {
        printf("Wrote %u bytes to %s\n", (uint32_t)written, fname_buf);
    } else {
        printf("Failed to write %s\n", fname_buf);
    }
    return 0;
}

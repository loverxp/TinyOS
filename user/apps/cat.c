/* cat.c - Display file contents (user-mode) */
#include <stdio.h>
#include <syscall.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    char args[256];
    get_cmdline(args, sizeof(args));

    if (args[0] == '\0') {
        printf("Usage: cat <filename>\n");
        return 1;
    }

    int fd = open(args, O_RDONLY);
    if (fd < 0) {
        printf("File not found: %s\n", args);
        return 1;
    }

    /* Get file size via stat */
    vfs_stat_t st;
    stat(args, &st);

    char buf[512];
    int total = 0;
    int n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("%s", buf);
        total += n;
    }

    if (total > 0 && buf[total - 1 < sizeof(buf) ? total - 1 : sizeof(buf) - 1] != '\n') {
        printf("\n");
    }
    printf("(%u bytes)\n", st.size);

    close(fd);
    return 0;
}

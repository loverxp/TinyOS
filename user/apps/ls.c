/* ls.c - List directory contents (user-mode) */
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    char args[256];
    get_cmdline(args, sizeof(args));

    /* Default to root directory */
    const char* path = "";
    if (args[0] != '\0') {
        path = args;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        /* Try opening root if no path given */
        if (path[0] == '\0') {
            fd = open("/", O_RDONLY);
        }
        if (fd < 0) {
            printf("ls: cannot access '%s'\n", path);
            return 1;
        }
    }

    vfs_dirent_t entry;
    uint32_t index = 0;
    int count = 0;

    while (readdir(fd, &entry, index) == 0) {
        if (entry.type == FT_DIR) {
            printf("  %-12s  <DIR>\n", entry.name);
        } else {
            printf("  %-12s  %u bytes\n", entry.name, entry.size);
        }
        count++;
        index++;
    }

    if (count == 0) {
        printf("  (empty directory)\n");
    } else {
        printf("  %d item(s)\n", count);
    }

    close(fd);
    return 0;
}

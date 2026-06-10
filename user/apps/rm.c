/* rm.c - Delete file (user-mode) */
#include <stdio.h>
#include <syscall.h>
#include <unistd.h>

int main(void) {
    char args[256];
    get_cmdline(args, sizeof(args));

    if (args[0] == '\0') {
        printf("Usage: rm <filename>\n");
        return 1;
    }

    if (unlink(args) == 0) {
        printf("Deleted %s\n", args);
    } else {
        printf("File not found: %s\n", args);
    }
    return 0;
}

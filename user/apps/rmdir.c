/* rmdir.c - Remove directory (user-mode) */
#include <stdio.h>
#include <syscall.h>
#include <fcntl.h>

int main(void) {
    char args[256];
    get_cmdline(args, sizeof(args));

    if (args[0] == '\0') {
        printf("Usage: rmdir <dirname>\n");
        return 1;
    }

    if (rmdir(args) == 0) {
        printf("Directory removed: %s\n", args);
    } else {
        printf("Failed to remove directory: %s\n", args);
    }
    return 0;
}

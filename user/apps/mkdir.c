/* mkdir.c - Create directory (user-mode) */
#include <stdio.h>
#include <syscall.h>
#include <fcntl.h>

int main(void) {
    char args[256];
    get_cmdline(args, sizeof(args));

    if (args[0] == '\0') {
        printf("Usage: mkdir <dirname>\n");
        return 1;
    }

    if (mkdir(args) == 0) {
        printf("Directory created: %s\n", args);
    } else {
        printf("Failed to create directory: %s\n", args);
    }
    return 0;
}

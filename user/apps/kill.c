/* kill.c - Send signal to process (user-mode) */
#include <stdio.h>
#include <stdlib.h>
#include <syscall.h>

int main(void) {
    char args[256];
    get_cmdline(args, sizeof(args));

    if (args[0] == '\0') {
        printf("Usage: kill <pid> [signal]\n");
        return 1;
    }

    const char* p = args;
    int pid = 0;
    while (*p >= '0' && *p <= '9') pid = pid * 10 + (*p++ - '0');
    while (*p == ' ') p++;
    int sig = 15; /* SIGTERM default */
    if (*p >= '0' && *p <= '9') {
        sig = 0;
        while (*p >= '0' && *p <= '9') sig = sig * 10 + (*p++ - '0');
    }

    if (kill(pid, sig) == 0) {
        printf("Sent signal %d to pid %d\n", sig, pid);
    } else {
        printf("Failed to send signal (pid %d not found)\n", pid);
    }
    return 0;
}

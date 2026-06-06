/* forktest.c - Test fork() system call
 * Uses ONLY debug_print (syscall 6) for output, no printf complexity.
 */

#include "../libc/syscall.h"

/* Simple serial debug print helper */
static void debug_putstr(const char* s) {
    debug_print(s);
}

int main(void) {
    debug_putstr("ForkTest: before fork...\n");

    int pid = fork();

    /* Build a small message using direct serial output */
    if (pid == 0) {
        /* Child process */
        debug_putstr("CHILD: fork=0, running!\n");

        debug_putstr("CHILD: yielding...\n");
        asm volatile("mov $8, %%eax; int $0x80" : : : "eax", "memory");

        debug_putstr("CHILD: back from yield!\n");
    } else if (pid > 0) {
        /* Parent process */
        debug_putstr("PARENT: child PID=");
        {
            char buf[12];
            int n = pid;
            int i = 0;
            if (n == 0) { buf[i++] = '0'; }
            else {
                char tmp[12];
                int j = 0;
                while (n > 0) {
                    tmp[j++] = '0' + (n % 10);
                    n /= 10;
                }
                while (j > 0) buf[i++] = tmp[--j];
            }
            buf[i] = '\0';
            debug_print(buf);
        }
        debug_putstr("\n");

        debug_putstr("PARENT: yielding...\n");
        asm volatile("mov $8, %%eax; int $0x80" : : : "eax", "memory");

        debug_putstr("PARENT: back from yield!\n");
    } else {
        debug_putstr("ForkTest: fork FAILED!\n");
    }

    debug_putstr((pid == 0) ? "CHILD: exiting.\n" : "PARENT: exiting.\n");
    return 0;
}
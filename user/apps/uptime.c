/* uptime.c - User-space uptime command */

#include "../libc/syscall.h"
#include "../libc/stdio.h"

int main(void) {
    uint32_t ticks;
    if (get_system_info(0, &ticks, sizeof(ticks)) < sizeof(ticks)) {
        printf("uptime: system call failed\n");
        return 1;
    }
    uint32_t secs = ticks / 50;
    uint32_t ms = (ticks % 50) * 20;
    printf("Uptime: %u.%u seconds\n", secs, ms);
    return 0;
}
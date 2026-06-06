/* meminfo.c - User-space memory info command */

#include "../libc/syscall.h"
#include "../libc/stdio.h"

int main(void) {
    uint32_t meminfo[4]; /* total_pages, free_pages, used_pages, total_kb */
    int ret = get_system_info(3, meminfo, sizeof(meminfo));
    if (ret < (int)sizeof(uint32_t) * 4) {
        printf("meminfo: system call failed (ret=%d)\n", ret);
        return 1;
    }
    printf("Memory:\n");
    printf("  Total: %u MB (%u pages)\n", meminfo[3] / 1024, meminfo[0]);
    printf("  Used:  %u pages (%u KB)\n", meminfo[2], meminfo[2] * 4);
    printf("  Free:  %u pages (%u KB)\n", meminfo[1], meminfo[1] * 4);
    return 0;
}
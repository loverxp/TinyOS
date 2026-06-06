/* diskinfo.c - User-space disk info command */

#include "../libc/syscall.h"
#include "../libc/stdio.h"

int main(void) {
    uint32_t dinfo[5]; /* total_size, bytes_per_sector, sectors_per_cluster,
                          total_clusters, root_entry_count */
    int ret = get_system_info(4, dinfo, sizeof(dinfo));
    if (ret < (int)sizeof(uint32_t) * 5) {
        printf("diskinfo: system call failed\n");
        return 1;
    }
    printf("Disk info:\n");
    printf("  Total size:      %u KB\n", dinfo[0] / 1024);
    printf("  Bytes/sector:    %u\n", dinfo[1]);
    printf("  Sectors/cluster: %u\n", dinfo[2]);
    printf("  Total clusters:  %u\n", dinfo[3]);
    printf("  Root entries:    %u\n", dinfo[4]);
    return 0;
}
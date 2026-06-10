/* arp.c - Show/clear ARP cache (user-mode) */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>

typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    uint8_t  valid;
    uint8_t  pad[1];
} arp_entry_info_t;

int main(void) {
    char args[256];
    get_cmdline(args, sizeof(args));

    if (strcmp(args, "-c") == 0 || strcmp(args, "clear") == 0) {
        net_arp_flush();
        printf("ARP cache cleared.\n");
        return 0;
    }

    arp_entry_info_t entries[16];
    int bytes = get_system_info(8, entries, sizeof(entries));
    int count = bytes > 0 ? bytes / sizeof(arp_entry_info_t) : 0;

    printf("ARP cache:\n");
    if (count == 0) {
        printf("  (empty)\n");
    } else {
        for (int i = 0; i < count; i++) {
            printf("  %u.%u.%u.%u -> %02x:%02x:%02x:%02x:%02x:%02x\n",
                   entries[i].ip & 0xFF, (entries[i].ip >> 8) & 0xFF,
                   (entries[i].ip >> 16) & 0xFF, (entries[i].ip >> 24) & 0xFF,
                   entries[i].mac[0], entries[i].mac[1], entries[i].mac[2],
                   entries[i].mac[3], entries[i].mac[4], entries[i].mac[5]);
        }
    }
    return 0;
}

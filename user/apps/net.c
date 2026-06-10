/* net.c - Show network configuration (user-mode) */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>

int main(void) {
    uint8_t nbuf[18]; /* 3*uint32_t + 6 bytes MAC */
    int bytes = get_system_info(6, nbuf, sizeof(nbuf));
    if (bytes < 12) {
        printf("Network not initialized\n");
        return 1;
    }

    uint32_t ip, gw, mask;
    memcpy(&ip, nbuf, 4);
    memcpy(&gw, nbuf + 4, 4);
    memcpy(&mask, nbuf + 8, 4);

    if (ip == 0) {
        printf("Network not initialized\n");
        return 1;
    }

    printf("Network config:\n");
    printf("  IP:      %u.%u.%u.%u\n", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
    printf("  Gateway: %u.%u.%u.%u\n", gw & 0xFF, (gw >> 8) & 0xFF, (gw >> 16) & 0xFF, (gw >> 24) & 0xFF);
    printf("  Mask:    %u.%u.%u.%u\n", mask & 0xFF, (mask >> 8) & 0xFF, (mask >> 16) & 0xFF, (mask >> 24) & 0xFF);

    if (bytes >= 18) {
        printf("  MAC:     %02x:%02x:%02x:%02x:%02x:%02x\n",
               nbuf[12], nbuf[13], nbuf[14], nbuf[15], nbuf[16], nbuf[17]);
    }
    return 0;
}

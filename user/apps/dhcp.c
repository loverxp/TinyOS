/* dhcp.c - Trigger DHCP discovery (user-mode) */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>

int main(void) {
    printf("Sending DHCP Discover...\n");
    int ret = net_dhcp();
    if (ret == 0) {
        /* Wait a bit for DHCP to complete, then show results */
        sleep_ms(5000);
        uint8_t nbuf[18];
        int bytes = get_system_info(6, nbuf, sizeof(nbuf));
        if (bytes >= 12) {
            uint32_t ip, gw, mask;
            memcpy(&ip, nbuf, 4);
            memcpy(&gw, nbuf + 4, 4);
            memcpy(&mask, nbuf + 8, 4);
            printf("DHCP result:\n");
            printf("  IP:      %u.%u.%u.%u\n", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
            printf("  Gateway: %u.%u.%u.%u\n", gw & 0xFF, (gw >> 8) & 0xFF, (gw >> 16) & 0xFF, (gw >> 24) & 0xFF);
            printf("  Mask:    %u.%u.%u.%u\n", mask & 0xFF, (mask >> 8) & 0xFF, (mask >> 16) & 0xFF, (mask >> 24) & 0xFF);
        }
    } else {
        printf("DHCP failed.\n");
    }
    return 0;
}

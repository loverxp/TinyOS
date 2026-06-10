/* ping.c - Send ICMP ping (user-mode) */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>

/* Parse "A.B.C.D" into uint32_t (little-endian). Returns 0 on failure. */
static uint32_t parse_ip(const char* s) {
    uint32_t octets[4] = {0};
    int n = 0;
    while (*s && n < 4) {
        if (*s >= '0' && *s <= '9') {
            octets[n] = octets[n] * 10 + (*s - '0');
        } else if (*s == '.') {
            n++;
        } else {
            break;
        }
        s++;
    }
    if (n != 3) return 0;
    for (int i = 0; i < 4; i++) {
        if (octets[i] > 255) return 0;
    }
    return octets[0] | (octets[1] << 8) | (octets[2] << 16) | (octets[3] << 24);
}

int main(void) {
    char args[256];
    get_cmdline(args, sizeof(args));

    if (args[0] == '\0') {
        printf("Usage: ping <ip|hostname>\n");
        return 1;
    }

    uint32_t target_ip = parse_ip(args);
    if (target_ip == 0) {
        /* Try DNS */
        if (net_dns_resolve(args, &target_ip) != 0) {
            printf("Could not resolve: %s\n", args);
            return 1;
        }
        printf("Resolved %s -> %u.%u.%u.%u\n", args,
               target_ip & 0xFF, (target_ip >> 8) & 0xFF,
               (target_ip >> 16) & 0xFF, (target_ip >> 24) & 0xFF);
    }

    printf("Pinging %u.%u.%u.%u...\n",
           target_ip & 0xFF, (target_ip >> 8) & 0xFF,
           (target_ip >> 16) & 0xFF, (target_ip >> 24) & 0xFF);

    int ret = net_ping(target_ip, 1, 1);
    if (ret == 0) {
        printf("Ping sent to %u.%u.%u.%u\n",
               target_ip & 0xFF, (target_ip >> 8) & 0xFF,
               (target_ip >> 16) & 0xFF, (target_ip >> 24) & 0xFF);
    } else {
        printf("Ping failed (ARP resolution needed, retry in a moment)\n");
    }
    return 0;
}

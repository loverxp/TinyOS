/* netstat.c - Show network statistics (user-mode) */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <syscall.h>

typedef struct {
    uint32_t rx_packets;
    uint32_t tx_packets;
    uint32_t rx_errors;
    uint32_t tx_errors;
    uint32_t arp_requests_sent;
    uint32_t arp_replies_recv;
    uint32_t icmp_sent;
    uint32_t icmp_recv;
    uint32_t udp_sent;
    uint32_t udp_recv;
    uint32_t tcp_sent;
    uint32_t tcp_recv;
} net_stats_info_t;

int main(void) {
    char args[256];
    get_cmdline(args, sizeof(args));

    if (strcmp(args, "-r") == 0) {
        net_reset_stats();
        printf("Network statistics reset.\n");
        return 0;
    }

    net_stats_info_t s;
    int bytes = get_system_info(7, &s, sizeof(s));
    if (bytes <= 0) {
        printf("No network statistics available.\n");
        return 1;
    }

    printf("Network statistics:\n");
    printf("  RX packets: %u\n", s.rx_packets);
    printf("  TX packets: %u\n", s.tx_packets);
    printf("  RX errors:  %u\n", s.rx_errors);
    printf("  TX errors:  %u\n", s.tx_errors);
    printf("  ARP:  %u requests sent, %u replies recv\n", s.arp_requests_sent, s.arp_replies_recv);
    printf("  ICMP: %u sent, %u recv\n", s.icmp_sent, s.icmp_recv);
    printf("  UDP:  %u sent, %u recv\n", s.udp_sent, s.udp_recv);
    printf("  TCP:  %u sent, %u recv\n", s.tcp_sent, s.tcp_recv);
    return 0;
}

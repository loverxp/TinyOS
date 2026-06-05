#ifndef NET_H
#define NET_H

#include "types.h"

/* IP address helper */
#define IP4(a,b,c,d) (((uint32_t)(a)) | ((uint32_t)(b)<<8) | ((uint32_t)(c)<<16) | ((uint32_t)(d)<<24))

/* Byte order conversion (network = big-endian) */
static inline uint16_t htons(uint16_t x) {
    return (x >> 8) | (x << 8);
}
static inline uint16_t ntohs(uint16_t x) { return htons(x); }
static inline uint32_t htonl(uint32_t x) {
    return ((x >> 24) & 0xFF) |
           ((x >> 8)  & 0xFF00) |
           ((x << 8)  & 0xFF0000) |
           ((x << 24) & 0xFF000000);
}
static inline uint32_t ntohl(uint32_t x) { return htonl(x); }

/* Ethernet header */
typedef struct {
    uint8_t  dst_mac[6];
    uint8_t  src_mac[6];
    uint16_t ethertype;     /* Network byte order */
} __attribute__((packed)) eth_header_t;

#define ETHERTYPE_ARP   0x0806
#define ETHERTYPE_IP    0x0800

/* ARP header (for IPv4 over Ethernet) */
typedef struct {
    uint16_t hw_type;       /* 1 = Ethernet */
    uint16_t proto_type;    /* 0x0800 = IPv4 */
    uint8_t  hw_len;        /* 6 = MAC */
    uint8_t  proto_len;     /* 4 = IPv4 */
    uint16_t opcode;        /* 1 = request, 2 = reply */
    uint8_t  sender_mac[6];
    uint32_t sender_ip;
    uint8_t  target_mac[6];
    uint32_t target_ip;
} __attribute__((packed)) arp_header_t;

#define ARP_OP_REQUEST  1
#define ARP_OP_REPLY    2

/* IP header */
typedef struct {
    uint8_t  version_ihl;   /* Version (4 bits) + IHL (4 bits) */
    uint8_t  tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_frag;    /* Flags (3 bits) + Fragment offset (13 bits) */
    uint8_t  ttl;
    uint8_t  protocol;      /* 17 = UDP, 6 = TCP */
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} __attribute__((packed)) ip_header_t;

#define IP_PROTO_UDP    17
#define IP_PROTO_ICMP   1
#define IP_PROTO_TCP    6

/* UDP header */
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;        /* Header + data */
    uint16_t checksum;      /* Optional in IPv4 */
} __attribute__((packed)) udp_header_t;

/* ICMP header (for ping) */
typedef struct {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
} __attribute__((packed)) icmp_header_t;

#define ICMP_ECHO_REPLY   0
#define ICMP_ECHO_REQUEST 8

/* ARP table entry */
typedef struct {
    uint32_t ip;
    uint8_t  mac[6];
    int      valid;
} arp_entry_t;

#define ARP_TABLE_SIZE 16

/* UDP receive callback */
typedef void (*udp_recv_callback_t)(uint32_t src_ip, uint16_t src_port,
                                     uint16_t dst_port,
                                     const uint8_t* data, uint16_t len);

/* ---- TCP ---- */

/* TCP header */
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;   /* High 4 bits: header length in 32-bit words */
    uint8_t  flags;
    uint16_t window_size;
    uint16_t checksum;
    uint16_t urgent_ptr;
} __attribute__((packed)) tcp_header_t;

/* TCP flags */
#define TCP_FIN   0x01
#define TCP_SYN   0x02
#define TCP_RST   0x04
#define TCP_PSH   0x08
#define TCP_ACK   0x10
#define TCP_URG   0x20

/* TCP connection states */
#define TCP_LISTEN      0
#define TCP_SYN_RCVD    1
#define TCP_ESTABLISHED 2
#define TCP_CLOSE_WAIT  3
#define TCP_LAST_ACK    4
#define TCP_CLOSED      5

/* TCP connection entry */
typedef struct {
    int      used;
    uint32_t ip;
    uint16_t port;
    uint32_t seq;           /* Next expected sequence number from peer */
    uint32_t ack_seq;       /* Next ack number we will send */
    int      state;
} tcp_conn_t;

#define TCP_CONN_MAX 4

/* TCP receive callback for established connections */
typedef void (*tcp_recv_callback_t)(uint32_t src_ip, uint16_t src_port,
                                    const uint8_t* data, uint16_t len);

/* Network statistics */
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
} net_stats_t;

/* API */
void net_init(uint32_t ip_addr, uint32_t gateway, uint32_t subnet_mask);
void net_recv_handler(const uint8_t* frame, uint16_t len);
int net_send_udp(uint32_t dst_ip, uint16_t dst_port,
                 uint16_t src_port, const void* data, uint16_t len);
int net_send_arp_request(uint32_t target_ip);
int net_send_icmp_echo(uint32_t dst_ip, uint16_t id, uint16_t seq);
void net_set_udp_callback(udp_recv_callback_t cb);
const arp_entry_t* net_arp_lookup(uint32_t ip);
void net_get_config(uint32_t* ip, uint32_t* gateway, uint32_t* mask);

/* ARP table access */
const arp_entry_t* net_arp_table_get(int index);  /* returns NULL for invalid index */
void net_arp_clear(void);

/* Statistics */
const net_stats_t* net_get_stats(void);
void net_stats_reset(void);

/* TCP API */
void net_tcp_listen(uint16_t port);
void net_set_tcp_callback(tcp_recv_callback_t cb);
int  net_tcp_send(uint32_t dst_ip, uint16_t dst_port,
                  const void* data, uint16_t len, uint8_t flags);
int  net_tcp_close(uint32_t dst_ip, uint16_t dst_port);

#endif /* NET_H */

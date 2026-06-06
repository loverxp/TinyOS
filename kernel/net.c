#include "../include/net.h"
#include "../include/ne2000.h"
#include "../include/stdio.h"
#include "../include/string.h"
#include "../include/timer.h"

/* Network configuration (host byte order) */
static uint32_t my_ip = 0;
static uint32_t my_gateway = 0;
static uint32_t my_mask = 0;

/* ARP table */
static arp_entry_t arp_table[ARP_TABLE_SIZE];

/* IP identification counter */
static uint16_t ip_id_counter = 0;

/* UDP receive callback */
static udp_recv_callback_t udp_callback = NULL;

/* Broadcast MAC */
static const uint8_t BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* Network statistics */
static net_stats_t stats;

/* ---- Checksum ---- */

static uint16_t ip_checksum(const void* data, uint16_t len) {
    const uint16_t* words = (const uint16_t*)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *words++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(const uint8_t*)words;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

/* ---- ARP Table ---- */

static void arp_table_add(uint32_t ip, const uint8_t* mac) {
    /* Update existing entry */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            memcpy(arp_table[i].mac, mac, 6);
            return;
        }
    }
    /* Add new entry */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (!arp_table[i].valid) {
            arp_table[i].ip = ip;
            memcpy(arp_table[i].mac, mac, 6);
            arp_table[i].valid = 1;
            serial_printf("[ARP] Added %u.%u.%u.%u -> %02x:%02x:%02x:%02x:%02x:%02x\n",
                          ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF,
                          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            return;
        }
    }
}

const arp_entry_t* net_arp_lookup(uint32_t ip) {
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (arp_table[i].valid && arp_table[i].ip == ip) {
            return &arp_table[i];
        }
    }
    return NULL;
}

/* ---- Ethernet Frame Builder ---- */

static uint8_t tx_frame[1600];

static void build_eth_header(const uint8_t* dst_mac, uint16_t ethertype) {
    eth_header_t* eth = (eth_header_t*)tx_frame;
    memcpy(eth->dst_mac, dst_mac, 6);
    memcpy(eth->src_mac, ne2000_get_mac(), 6);
    eth->ethertype = htons(ethertype);
}

/* ---- ARP ---- */

int net_send_arp_request(uint32_t target_ip) {
    build_eth_header(BROADCAST_MAC, ETHERTYPE_ARP);

    arp_header_t* arp = (arp_header_t*)(tx_frame + sizeof(eth_header_t));
    arp->hw_type = htons(1);
    arp->proto_type = htons(0x0800);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->opcode = htons(ARP_OP_REQUEST);
    memcpy(arp->sender_mac, ne2000_get_mac(), 6);
    arp->sender_ip = my_ip;
    memset(arp->target_mac, 0, 6);
    arp->target_ip = target_ip;

    uint16_t frame_len = sizeof(eth_header_t) + sizeof(arp_header_t);
    if (frame_len < 60) frame_len = 60;

    serial_printf("[ARP] Sending request for %u.%u.%u.%u\n",
                  target_ip & 0xFF, (target_ip >> 8) & 0xFF,
                  (target_ip >> 16) & 0xFF, (target_ip >> 24) & 0xFF);

    stats.arp_requests_sent++;
    return ne2000_send(tx_frame, frame_len);
}

static void handle_arp(const uint8_t* data, uint16_t len) {
    if (len < sizeof(arp_header_t)) {
        serial_printf("[ARP] recv: too short (%u bytes)\n", len);
        return;
    }

    const arp_header_t* arp = (const arp_header_t*)data;

    if (ntohs(arp->hw_type) != 1 || ntohs(arp->proto_type) != 0x0800) {
        serial_printf("[ARP] recv: unsupported hw=%u proto=0x%04X\n",
                      ntohs(arp->hw_type), ntohs(arp->proto_type));
        return;
    }
    if (arp->hw_len != 6 || arp->proto_len != 4) return;

    uint16_t opcode = ntohs(arp->opcode);
    serial_printf("[ARP] recv: op=%s sender=%u.%u.%u.%u target=%u.%u.%u.%u "
                  "sender_mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
                  (opcode == ARP_OP_REQUEST) ? "REQUEST" :
                  (opcode == ARP_OP_REPLY) ? "REPLY" : "UNKNOWN",
                  arp->sender_ip & 0xFF, (arp->sender_ip >> 8) & 0xFF,
                  (arp->sender_ip >> 16) & 0xFF, (arp->sender_ip >> 24) & 0xFF,
                  arp->target_ip & 0xFF, (arp->target_ip >> 8) & 0xFF,
                  (arp->target_ip >> 16) & 0xFF, (arp->target_ip >> 24) & 0xFF,
                  arp->sender_mac[0], arp->sender_mac[1], arp->sender_mac[2],
                  arp->sender_mac[3], arp->sender_mac[4], arp->sender_mac[5]);

    /* Add sender to ARP table */
    arp_table_add(arp->sender_ip, arp->sender_mac);
    if (opcode == ARP_OP_REPLY) stats.arp_replies_recv++;

    if (opcode == ARP_OP_REQUEST && arp->target_ip == my_ip) {
        serial_printf("[ARP] Received request for us, sending reply\n");
        /* Send ARP reply */
        build_eth_header(arp->sender_mac, ETHERTYPE_ARP);

        arp_header_t* reply = (arp_header_t*)(tx_frame + sizeof(eth_header_t));
        reply->hw_type = htons(1);
        reply->proto_type = htons(0x0800);
        reply->hw_len = 6;
        reply->proto_len = 4;
        reply->opcode = htons(ARP_OP_REPLY);
        memcpy(reply->sender_mac, ne2000_get_mac(), 6);
        reply->sender_ip = my_ip;
        memcpy(reply->target_mac, arp->sender_mac, 6);
        reply->target_ip = arp->sender_ip;

        uint16_t frame_len = sizeof(eth_header_t) + sizeof(arp_header_t);
        ne2000_send(tx_frame, frame_len);
    }
}

/* ---- IP ---- */

static void build_ip_header(ip_header_t* ip, uint32_t dst_ip, uint8_t protocol,
                            uint16_t payload_len) {
    ip->version_ihl = 0x45;   /* IPv4, 20-byte header */
    ip->tos = 0;
    ip->total_length = htons(20 + payload_len);
    ip->identification = htons(ip_id_counter++);
    ip->flags_frag = htons(0x4000);  /* Don't fragment */
    ip->ttl = 64;
    ip->protocol = protocol;
    ip->src_ip = my_ip;
    ip->dst_ip = dst_ip;
    ip->checksum = 0;
    ip->checksum = ip_checksum(ip, 20);
}

/* Resolve destination: use gateway for off-subnet, direct for on-subnet */
static uint32_t resolve_dst(uint32_t dst_ip) {
    if ((dst_ip & my_mask) != (my_ip & my_mask)) {
        return my_gateway;
    }
    return dst_ip;
}

/* ---- ICMP (Ping) ---- */

int net_send_icmp_echo(uint32_t dst_ip, uint16_t id, uint16_t seq) {
    uint32_t route_ip = resolve_dst(dst_ip);
    const arp_entry_t* arp = net_arp_lookup(route_ip);
    if (!arp) {
        net_send_arp_request(route_ip);
        serial_printf("[ICMP] ARP pending for %u.%u.%u.%u\n",
                      route_ip & 0xFF, (route_ip >> 8) & 0xFF,
                      (route_ip >> 16) & 0xFF, (route_ip >> 24) & 0xFF);
        return -1;
    }

    build_eth_header(arp->mac, ETHERTYPE_IP);

    uint8_t* payload = tx_frame + sizeof(eth_header_t) + 20;  /* after IP header */
    icmp_header_t* icmp = (icmp_header_t*)payload;
    icmp->type = ICMP_ECHO_REQUEST;
    icmp->code = 0;
    icmp->identifier = htons(id);
    icmp->sequence = htons(seq);
    icmp->checksum = 0;

    uint16_t icmp_len = sizeof(icmp_header_t);
    icmp->checksum = ip_checksum(icmp, icmp_len);

    ip_header_t* ip = (ip_header_t*)(tx_frame + sizeof(eth_header_t));
    build_ip_header(ip, dst_ip, IP_PROTO_ICMP, icmp_len);

    stats.icmp_sent++;
    uint16_t frame_len = sizeof(eth_header_t) + 20 + icmp_len;
    return ne2000_send(tx_frame, frame_len);
}

static void handle_icmp(const uint8_t* data, uint16_t len, uint32_t src_ip) {
    if (len < sizeof(icmp_header_t)) return;

    const icmp_header_t* icmp = (const icmp_header_t*)data;

    if (icmp->type == ICMP_ECHO_REQUEST && my_ip != 0) {
        /* Send echo reply */
        uint32_t route_ip = resolve_dst(src_ip);
        const arp_entry_t* arp = net_arp_lookup(route_ip);
        if (!arp) return;

        build_eth_header(arp->mac, ETHERTYPE_IP);

        uint8_t* payload = tx_frame + sizeof(eth_header_t) + 20;
        memcpy(payload, data, len);
        icmp_header_t* reply = (icmp_header_t*)payload;
        reply->type = ICMP_ECHO_REPLY;
        reply->checksum = 0;
        reply->checksum = ip_checksum(reply, len);

        ip_header_t* ip = (ip_header_t*)(tx_frame + sizeof(eth_header_t));
        build_ip_header(ip, src_ip, IP_PROTO_ICMP, len);

        stats.icmp_recv++;
        uint16_t frame_len = sizeof(eth_header_t) + 20 + len;
        ne2000_send(tx_frame, frame_len);

        serial_printf("[ICMP] Echo reply -> %u.%u.%u.%u seq=%u\n",
                      src_ip & 0xFF, (src_ip >> 8) & 0xFF,
                      (src_ip >> 16) & 0xFF, (src_ip >> 24) & 0xFF,
                      ntohs(icmp->sequence));
    }
}

/* ---- UDP ---- */

int net_send_udp(uint32_t dst_ip, uint16_t dst_port,
                 uint16_t src_port, const void* data, uint16_t len) {
    uint32_t route_ip = resolve_dst(dst_ip);
    const arp_entry_t* arp = net_arp_lookup(route_ip);
    if (!arp) {
        net_send_arp_request(route_ip);
        serial_printf("[UDP] ARP pending for %u.%u.%u.%u\n",
                      route_ip & 0xFF, (route_ip >> 8) & 0xFF,
                      (route_ip >> 16) & 0xFF, (route_ip >> 24) & 0xFF);
        return -1;
    }

    build_eth_header(arp->mac, ETHERTYPE_IP);

    /* Build UDP header + data */
    uint8_t* udp_start = tx_frame + sizeof(eth_header_t) + 20;
    udp_header_t* udp = (udp_header_t*)udp_start;
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length = htons(sizeof(udp_header_t) + len);
    udp->checksum = 0;  /* Optional for IPv4 */

    memcpy(udp_start + sizeof(udp_header_t), data, len);

    uint16_t payload_len = sizeof(udp_header_t) + len;

    ip_header_t* ip = (ip_header_t*)(tx_frame + sizeof(eth_header_t));
    build_ip_header(ip, dst_ip, IP_PROTO_UDP, payload_len);

    stats.udp_sent++;
    uint16_t frame_len = sizeof(eth_header_t) + 20 + payload_len;
    return ne2000_send(tx_frame, frame_len);
}

static void handle_udp(const uint8_t* data, uint16_t len, uint32_t src_ip) {
    if (len < sizeof(udp_header_t)) return;

    const udp_header_t* udp = (const udp_header_t*)data;
    uint16_t data_len = ntohs(udp->length) - sizeof(udp_header_t);
    if (data_len > len - sizeof(udp_header_t)) data_len = len - sizeof(udp_header_t);

    if (udp_callback) {
        stats.udp_recv++;
        udp_callback(src_ip, ntohs(udp->src_port), ntohs(udp->dst_port),
                     data + sizeof(udp_header_t), data_len);
    }
}

/* ---- TCP ---- */

static tcp_conn_t tcp_conns[TCP_CONN_MAX];
static uint16_t tcp_listen_port = 0;
static tcp_recv_callback_t tcp_callback = NULL;
static uint32_t tcp_my_seq = 10000;  /* Initial sequence number */

/* TCP pseudo header for checksum */
typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t  zero;
    uint8_t  protocol;  /* 6 for TCP */
    uint16_t tcp_length;
} __attribute__((packed)) tcp_pseudo_header_t;

static uint16_t tcp_checksum(const void* tcp_seg, uint16_t tcp_len,
                              uint32_t src_ip, uint32_t dst_ip) {
    uint8_t buf[1600];
    tcp_pseudo_header_t* pseudo = (tcp_pseudo_header_t*)buf;
    pseudo->src_ip = src_ip;
    pseudo->dst_ip = dst_ip;
    pseudo->zero = 0;
    pseudo->protocol = IP_PROTO_TCP;
    pseudo->tcp_length = htons(tcp_len);
    memcpy(buf + sizeof(tcp_pseudo_header_t), tcp_seg, tcp_len);
    return ip_checksum(buf, sizeof(tcp_pseudo_header_t) + tcp_len);
}

static tcp_conn_t* tcp_find_conn(uint32_t ip, uint16_t port) {
    for (int i = 0; i < TCP_CONN_MAX; i++) {
        if (tcp_conns[i].used && tcp_conns[i].ip == ip && tcp_conns[i].port == port)
            return &tcp_conns[i];
    }
    return NULL;
}

static tcp_conn_t* tcp_new_conn(uint32_t ip, uint16_t port) {
    for (int i = 0; i < TCP_CONN_MAX; i++) {
        if (!tcp_conns[i].used) {
            tcp_conns[i].used = 1;
            tcp_conns[i].ip = ip;
            tcp_conns[i].port = port;
            tcp_conns[i].state = TCP_LISTEN;
            return &tcp_conns[i];
        }
    }
    return NULL;
}

static void tcp_free_conn(tcp_conn_t* conn) {
    conn->used = 0;
    conn->state = TCP_CLOSED;
}

static void tcp_send_segment(tcp_conn_t* conn, uint32_t dst_ip, uint16_t dst_port,
                              uint32_t seq, uint32_t ack, uint8_t flags,
                              const void* data, uint16_t data_len) {
    /* Resolve MAC */
    uint32_t route_ip = resolve_dst(dst_ip);
    const arp_entry_t* arp = net_arp_lookup(route_ip);
    if (!arp) {
        net_send_arp_request(route_ip);
        serial_printf("[TCP] ARP pending for %u.%u.%u.%u\n",
                      route_ip & 0xFF, (route_ip >> 8) & 0xFF,
                      (route_ip >> 16) & 0xFF, (route_ip >> 24) & 0xFF);
        return;
    }

    build_eth_header(arp->mac, ETHERTYPE_IP);

    /* Build TCP header */
    uint8_t* tcp_start = tx_frame + sizeof(eth_header_t) + 20;
    tcp_header_t* tcp = (tcp_header_t*)tcp_start;
    tcp->src_port = htons(tcp_listen_port);
    tcp->dst_port = htons(dst_port);
    tcp->seq_num = htonl(seq);
    tcp->ack_num = htonl(ack);
    tcp->data_offset = (5 << 4) | 0;  /* 20-byte TCP header */
    tcp->flags = flags;
    tcp->window_size = htons(4096);
    tcp->urgent_ptr = 0;

    uint16_t tcp_len = sizeof(tcp_header_t) + data_len;
    memcpy(tcp_start + sizeof(tcp_header_t), data, data_len);

    /* Calculate TCP checksum */
    tcp->checksum = 0;
    uint32_t my_ip_val;
    net_get_config(&my_ip_val, NULL, NULL);
    tcp->checksum = tcp_checksum(tcp_start, tcp_len, my_ip_val, dst_ip);

    /* Build IP header */
    ip_header_t* ip = (ip_header_t*)(tx_frame + sizeof(eth_header_t));
    build_ip_header(ip, dst_ip, IP_PROTO_TCP, tcp_len);

    uint16_t frame_len = sizeof(eth_header_t) + 20 + tcp_len;
    stats.tcp_sent++;
    ne2000_send(tx_frame, frame_len);
}

void net_tcp_listen(uint16_t port) {
    tcp_listen_port = port;
    memset(tcp_conns, 0, sizeof(tcp_conns));
    tcp_my_seq = 10000;
    serial_printf("[TCP] Listening on port %u\n", port);
}

void net_set_tcp_callback(tcp_recv_callback_t cb) {
    tcp_callback = cb;
}

int net_tcp_send(uint32_t dst_ip, uint16_t dst_port,
                  const void* data, uint16_t len, uint8_t flags) {
    tcp_conn_t* conn = tcp_find_conn(dst_ip, dst_port);
    if (!conn) return -1;

    tcp_send_segment(conn, dst_ip, dst_port,
                     conn->ack_seq, conn->seq,
                     flags | TCP_ACK, data, len);

    if (flags & TCP_SYN) {
        conn->seq = conn->ack_seq;  /* seq set correctly */
    }
    if (data || (flags & TCP_FIN)) {
        conn->ack_seq += len;
        if (flags & TCP_FIN) conn->ack_seq++;
    }

    return 0;
}

int net_tcp_close(uint32_t dst_ip, uint16_t dst_port) {
    tcp_conn_t* conn = tcp_find_conn(dst_ip, dst_port);
    if (!conn) return -1;

    serial_printf("[TCP] Closing connection %u.%u.%u.%u:%u\n",
                  dst_ip & 0xFF, (dst_ip >> 8) & 0xFF,
                  (dst_ip >> 16) & 0xFF, (dst_ip >> 24) & 0xFF,
                  dst_port);

    /* Send FIN */
    tcp_send_segment(conn, dst_ip, dst_port,
                     conn->ack_seq, conn->seq,
                     TCP_FIN | TCP_ACK, NULL, 0);

    conn->ack_seq++;
    conn->state = TCP_LAST_ACK;
    return 0;
}

static void handle_tcp(const uint8_t* data, uint16_t len, uint32_t src_ip) {
    if (len < sizeof(tcp_header_t)) return;

    const tcp_header_t* tcp = (const tcp_header_t*)data;
    uint16_t src_port = ntohs(tcp->src_port);
    uint16_t dst_port = ntohs(tcp->dst_port);
    uint8_t  flags = tcp->flags;
    uint32_t seq = ntohl(tcp->seq_num);
    uint32_t ack = ntohl(tcp->ack_num);
    uint8_t  header_len = ((tcp->data_offset >> 4) & 0x0F) * 4;
    uint16_t payload_len = (len >= header_len) ? len - header_len : 0;
    const uint8_t* payload = data + header_len;

    /* Only handle packets to our listening port */
    if (dst_port != tcp_listen_port) return;

    serial_printf("[TCP] recv: %u.%u.%u.%u:%u -> port %u flags=%s%s%s%s seq=%u ack=%u len=%u\n",
                  src_ip & 0xFF, (src_ip >> 8) & 0xFF,
                  (src_ip >> 16) & 0xFF, (src_ip >> 24) & 0xFF,
                  src_port, dst_port,
                  (flags & TCP_SYN) ? "SYN " : "",
                  (flags & TCP_ACK) ? "ACK " : "",
                  (flags & TCP_PSH) ? "PSH " : "",
                  (flags & TCP_FIN) ? "FIN " : "",
                  seq, ack, payload_len);

    /* Find or create connection */
    tcp_conn_t* conn = tcp_find_conn(src_ip, src_port);

    if (flags & TCP_SYN) {
        /* New connection request */
        if (!conn) {
            conn = tcp_new_conn(src_ip, src_port);
            if (!conn) {
                serial_printf("[TCP] Connection table full!\n");
                return;
            }
        }
        conn->seq = seq + 1;          /* Next expected seq */
        conn->ack_seq = tcp_my_seq++;  /* Our initial seq for this connection */

        serial_printf("[TCP] SYN received, sending SYN-ACK (my_seq=%u)\n", conn->ack_seq);
        tcp_send_segment(conn, src_ip, src_port,
                         conn->ack_seq, conn->seq,
                         TCP_SYN | TCP_ACK, NULL, 0);
        conn->state = TCP_SYN_RCVD;
        return;
    }

    if (!conn) return;

    if (flags & TCP_ACK && conn->state == TCP_SYN_RCVD) {
        /* Handshake complete */
        conn->state = TCP_ESTABLISHED;
        conn->ack_seq++;  /* SYN-ACK consumed one sequence number */
        serial_printf("[TCP] Connection established: %u.%u.%u.%u:%u\n",
                      src_ip & 0xFF, (src_ip >> 8) & 0xFF,
                      (src_ip >> 16) & 0xFF, (src_ip >> 24) & 0xFF,
                      src_port);
        return;
    }

    if (flags & TCP_FIN) {
        serial_printf("[TCP] FIN received\n");
        conn->seq = seq + 1;
        tcp_send_segment(conn, src_ip, src_port,
                         conn->ack_seq, conn->seq,
                         TCP_ACK, NULL, 0);
        conn->state = TCP_CLOSE_WAIT;
        tcp_free_conn(conn);
        return;
    }

    if (payload_len > 0 && conn->state == TCP_ESTABLISHED) {
        stats.tcp_recv++;
        conn->seq = seq + payload_len;

        /* Send ACK for received data */
        serial_printf("[TCP] ACKing %u bytes (next_seq=%u)\n", payload_len, conn->seq);
        tcp_send_segment(conn, src_ip, src_port,
                         conn->ack_seq, conn->seq,
                         TCP_ACK, NULL, 0);

        /* Call the callback */
        if (tcp_callback) {
            tcp_callback(src_ip, src_port, payload, payload_len);
        }
    }
}

/* ---- IP packet dispatch ---- */

static void handle_ip(const uint8_t* data, uint16_t len) {
    if (len < 20) return;

    const ip_header_t* ip = (const ip_header_t*)data;

    /* Validate version */
    if ((ip->version_ihl >> 4) != 4) return;

    uint8_t ihl = (ip->version_ihl & 0x0F) * 4;
    if (ihl < 20 || ihl > len) return;

    uint16_t total_len = ntohs(ip->total_length);
    if (total_len > len) total_len = len;

    uint16_t payload_len = total_len - ihl;
    const uint8_t* payload = data + ihl;

    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            handle_icmp(payload, payload_len, ip->src_ip);
            break;
        case IP_PROTO_UDP:
            handle_udp(payload, payload_len, ip->src_ip);
            break;
        case IP_PROTO_TCP:
            handle_tcp(payload, payload_len, ip->src_ip);
            break;
    }
}

/* ---- Main receive handler ---- */

void net_recv_handler(const uint8_t* frame, uint16_t len) {
    if (len < sizeof(eth_header_t)) {
        stats.rx_errors++;
        serial_printf("[NET] recv: frame too short (%u bytes)\n", len);
        return;
    }

    const eth_header_t* eth = (const eth_header_t*)frame;
    uint16_t ethertype = ntohs(eth->ethertype);
    const uint8_t* payload = frame + sizeof(eth_header_t);
    uint16_t payload_len = len - sizeof(eth_header_t);

    stats.rx_packets++;

    serial_printf("[NET] recv: len=%u dst=%02x:%02x:%02x:%02x:%02x:%02x "
                  "src=%02x:%02x:%02x:%02x:%02x:%02x ethertype=0x%04x\n",
                  len,
                  eth->dst_mac[0], eth->dst_mac[1], eth->dst_mac[2],
                  eth->dst_mac[3], eth->dst_mac[4], eth->dst_mac[5],
                  eth->src_mac[0], eth->src_mac[1], eth->src_mac[2],
                  eth->src_mac[3], eth->src_mac[4], eth->src_mac[5],
                  ethertype);

    switch (ethertype) {
        case ETHERTYPE_ARP:
            serial_printf("[NET] recv: -> ARP handler\n");
            handle_arp(payload, payload_len);
            break;
        case ETHERTYPE_IP:
            serial_printf("[NET] recv: -> IP handler\n");
            /* Add source MAC + IP to ARP table so we can reply immediately
             * without needing a separate ARP exchange. This is critical for
             * TCP: a SYN-ACK must be sent back, and it needs the peer's MAC. */
            if (payload_len >= 20) {
                const ip_header_t* ip_hdr = (const ip_header_t*)payload;
                arp_table_add(ip_hdr->src_ip, eth->src_mac);
            }
            handle_ip(payload, payload_len);
            break;
        default:
            serial_printf("[NET] recv: unknown ethertype 0x%04x\n", ethertype);
            break;
    }
}

/* ---- Init ---- */

void net_init(uint32_t ip_addr, uint32_t gateway, uint32_t subnet_mask) {
    my_ip = ip_addr;
    my_gateway = gateway;
    my_mask = subnet_mask;

    memset(arp_table, 0, sizeof(arp_table));
    ip_id_counter = 0;
    memset(&stats, 0, sizeof(stats));

    serial_printf("[NET] IP: %u.%u.%u.%u, GW: %u.%u.%u.%u, Mask: %u.%u.%u.%u\n",
                  ip_addr & 0xFF, (ip_addr >> 8) & 0xFF, (ip_addr >> 16) & 0xFF, (ip_addr >> 24) & 0xFF,
                  gateway & 0xFF, (gateway >> 8) & 0xFF, (gateway >> 16) & 0xFF, (gateway >> 24) & 0xFF,
                  subnet_mask & 0xFF, (subnet_mask >> 8) & 0xFF, (subnet_mask >> 16) & 0xFF, (subnet_mask >> 24) & 0xFF);

    /* Pre-populate ARP with gateway (QEMU user-mode networking) */
    /* The gateway MAC will be learned via ARP on first packet */

    /* Initialize socket layer */
    net_socket_init();
}

void net_set_udp_callback(udp_recv_callback_t cb) {
    udp_callback = cb;
}

void net_get_config(uint32_t* ip, uint32_t* gateway, uint32_t* mask) {
    if (ip) *ip = my_ip;
    if (gateway) *gateway = my_gateway;
    if (mask) *mask = my_mask;
}

/* ARP table access */
const arp_entry_t* net_arp_table_get(int index) {
    if (index < 0 || index >= ARP_TABLE_SIZE) return NULL;
    if (!arp_table[index].valid) return NULL;
    return &arp_table[index];
}

void net_arp_clear(void) {
    memset(arp_table, 0, sizeof(arp_table));
}

/* Statistics */
const net_stats_t* net_get_stats(void) {
    return &stats;
}

void net_stats_reset(void) {
    memset(&stats, 0, sizeof(stats));
}

/* ---- DHCP Client ---- */

#define DHCP_MAGIC_COOKIE  0x63825363  /* in network byte order */

/* Send a raw UDP broadcast (src_ip may be 0 for DHCP Discover) */
static int dhcp_send_raw(uint32_t src_ip, uint16_t src_port,
                          uint16_t dst_port,
                          const void* data, uint16_t len) {
    build_eth_header(BROADCAST_MAC, ETHERTYPE_IP);

    uint8_t* udp_start = tx_frame + sizeof(eth_header_t) + 20;
    udp_header_t* udp = (udp_header_t*)udp_start;
    udp->src_port = htons(src_port);
    udp->dst_port = htons(dst_port);
    udp->length = htons(sizeof(udp_header_t) + len);
    udp->checksum = 0;

    memcpy(udp_start + sizeof(udp_header_t), data, len);

    uint16_t payload_len = sizeof(udp_header_t) + len;

    /* Build IP header manually (src_ip might be 0) */
    ip_header_t* ip = (ip_header_t*)(tx_frame + sizeof(eth_header_t));
    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = htons(20 + payload_len);
    ip->identification = htons(ip_id_counter++);
    ip->flags_frag = htons(0x4000);
    ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP;
    ip->src_ip = src_ip;
    ip->dst_ip = 0xFFFFFFFF;  /* broadcast */
    ip->checksum = 0;
    ip->checksum = ip_checksum(ip, 20);

    uint16_t frame_len = sizeof(eth_header_t) + 20 + payload_len;
    return ne2000_send(tx_frame, frame_len);
}

/* DHCP message types */
#define DHCP_DISCOVER 1
#define DHCP_OFFER    2
#define DHCP_REQUEST  3
#define DHCP_ACK      5

/* Build minimal DHCP packet */
static uint16_t build_dhcp_packet(uint8_t* buf, uint8_t msg_type,
                                   uint32_t xid, uint32_t requested_ip,
                                   uint32_t server_ip) {
    memset(buf, 0, 300);
    buf[0] = 1;         /* op: boot request */
    buf[1] = 1;         /* htype: ethernet */
    buf[2] = 6;         /* hlen: 6 bytes MAC */
    buf[3] = 0;         /* hops */
    /* xid */
    buf[4] = (xid >> 24) & 0xFF;
    buf[5] = (xid >> 16) & 0xFF;
    buf[6] = (xid >> 8)  & 0xFF;
    buf[7] = xid & 0xFF;
    /* secs, flags (broadcast) */
    buf[10] = 0x80;     /* flags high: broadcast */
    /* chaddr (offset 28) */
    memcpy(buf + 28, ne2000_get_mac(), 6);
    /* Magic cookie (offset 236) */
    buf[236] = 0x63;
    buf[237] = 0x82;
    buf[238] = 0x53;
    buf[239] = 0x63;
    /* Option 53: DHCP message type */
    uint16_t pos = 240;
    buf[pos++] = 53;    /* option code */
    buf[pos++] = 1;     /* length */
    buf[pos++] = msg_type;

    if (msg_type == DHCP_REQUEST) {
        /* Option 50: Requested IP */
        if (requested_ip) {
            buf[pos++] = 50;
            buf[pos++] = 4;
            buf[pos++] = requested_ip & 0xFF;
            buf[pos++] = (requested_ip >> 8) & 0xFF;
            buf[pos++] = (requested_ip >> 16) & 0xFF;
            buf[pos++] = (requested_ip >> 24) & 0xFF;
        }
        /* Option 54: Server Identifier */
        if (server_ip) {
            buf[pos++] = 54;
            buf[pos++] = 4;
            buf[pos++] = server_ip & 0xFF;
            buf[pos++] = (server_ip >> 8) & 0xFF;
            buf[pos++] = (server_ip >> 16) & 0xFF;
            buf[pos++] = (server_ip >> 24) & 0xFF;
        }
    }

    /* Option 61: Client Identifier (MAC) */
    buf[pos++] = 61;
    buf[pos++] = 7;
    buf[pos++] = 1;  /* type: ethernet */
    memcpy(buf + pos, ne2000_get_mac(), 6);
    pos += 6;

    /* Option 55: Parameter Request List */
    buf[pos++] = 55;
    buf[pos++] = 3;
    buf[pos++] = 1;   /* subnet mask */
    buf[pos++] = 3;   /* router (gateway) */
    buf[pos++] = 51;  /* lease time */

    /* End option */
    buf[pos++] = 255;

    return pos;
}

/* Parse DHCP options from a reply */
static int parse_dhcp_options(const uint8_t* buf, uint16_t len,
                               uint32_t* out_ip, uint32_t* out_mask,
                               uint32_t* out_gw, uint32_t* out_server,
                               uint8_t* out_msg_type) {
    /* Skip to options (after magic cookie at offset 236) */
    uint16_t pos = 240;
    *out_msg_type = 0;
    (void)out_ip;  /* offered IP is read from yiaddr separately */

    while (pos < len) {
        uint8_t opt = buf[pos++];
        if (opt == 255) break;   /* End */
        if (opt == 0) continue;  /* Pad */
        if (pos >= len) break;
        uint8_t olen = buf[pos++];
        if (pos + olen > len) break;

        if (opt == 53 && olen == 1) {
            *out_msg_type = buf[pos];
        } else if (opt == 1 && olen == 4) {
            *out_mask = buf[pos] | (buf[pos+1] << 8) |
                        (buf[pos+2] << 16) | (buf[pos+3] << 24);
        } else if (opt == 3 && olen == 4) {
            *out_gw = buf[pos] | (buf[pos+1] << 8) |
                      (buf[pos+2] << 16) | (buf[pos+3] << 24);
        } else if (opt == 54 && olen == 4) {
            *out_server = buf[pos] | (buf[pos+1] << 8) |
                          (buf[pos+2] << 16) | (buf[pos+3] << 24);
        }
        pos += olen;
    }
    return (*out_msg_type != 0) ? 0 : -1;
}

/* State for DHCP transaction */
static uint32_t dhcp_xid = 0;
static uint32_t dhcp_offered_ip = 0;
static uint32_t dhcp_server_ip = 0;
static uint8_t  dhcp_state = 0;  /* 0=idle, 1=sent discover, 2=got offer */

/* DHCP recv handler (called from handle_udp on port 68) */
static void dhcp_recv_handler(uint32_t src_ip, uint16_t src_port,
                               uint16_t dst_port,
                               const uint8_t* data, uint16_t len) {
    (void)src_ip; (void)src_port; (void)dst_port;
    if (len < 240) return;

    /* Verify xid */
    uint32_t xid = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) |
                   ((uint32_t)data[6] << 8) | data[7];
    if (xid != dhcp_xid) return;

    /* Verify magic cookie */
    if (data[236] != 0x63 || data[237] != 0x82 ||
        data[238] != 0x53 || data[239] != 0x63) return;

    /* yiaddr = offered IP (offset 16) */
    uint32_t offered = data[16] | (data[17] << 8) |
                       (data[18] << 16) | (data[19] << 24);

    uint32_t mask = 0, gw = 0, server = 0;
    uint8_t msg_type = 0;
    parse_dhcp_options(data, len, &offered, &mask, &gw, &server, &msg_type);

    if (msg_type == DHCP_OFFER && dhcp_state == 1) {
        dhcp_offered_ip = offered;
        dhcp_server_ip = server;
        dhcp_state = 2;
        serial_printf("[DHCP] Offer: IP=%u.%u.%u.%u server=%u.%u.%u.%u\n",
                      offered & 0xFF, (offered >> 8) & 0xFF,
                      (offered >> 16) & 0xFF, (offered >> 24) & 0xFF,
                      server & 0xFF, (server >> 8) & 0xFF,
                      (server >> 16) & 0xFF, (server >> 24) & 0xFF);
    } else if (msg_type == DHCP_ACK && dhcp_state == 3) {
        my_ip = offered;
        my_mask = mask;
        my_gateway = gw;
        dhcp_state = 4;  /* done */
        serial_printf("[DHCP] ACK: IP=%u.%u.%u.%u mask=%u.%u.%u.%u gw=%u.%u.%u.%u\n",
                      offered & 0xFF, (offered >> 8) & 0xFF,
                      (offered >> 16) & 0xFF, (offered >> 24) & 0xFF,
                      mask & 0xFF, (mask >> 8) & 0xFF,
                      (mask >> 16) & 0xFF, (mask >> 24) & 0xFF,
                      gw & 0xFF, (gw >> 8) & 0xFF,
                      (gw >> 16) & 0xFF, (gw >> 24) & 0xFF);
    }
}

int net_dhcp_discover(void) {
    dhcp_xid = 0xDEADBEEF;  /* simple fixed xid */
    dhcp_state = 1;
    dhcp_offered_ip = 0;
    dhcp_server_ip = 0;

    /* Install DHCP recv handler */
    udp_recv_callback_t old_cb = udp_callback;
    udp_callback = dhcp_recv_handler;

    uint8_t pkt[300];
    uint16_t pkt_len = build_dhcp_packet(pkt, DHCP_DISCOVER, dhcp_xid, 0, 0);

    serial_printf("[DHCP] Sending Discover (%u bytes)\n", pkt_len);
    dhcp_send_raw(0, 68, 67, pkt, pkt_len);

    /* Wait for offer (poll) */
    for (int i = 0; i < 100 && dhcp_state < 2; i++) {
        ne2000_poll_recv();
    }

    if (dhcp_state < 2) {
        serial_printf("[DHCP] No offer received\n");
        udp_callback = old_cb;
        return -1;
    }

    /* Send Request */
    dhcp_state = 3;
    pkt_len = build_dhcp_packet(pkt, DHCP_REQUEST, dhcp_xid,
                                 dhcp_offered_ip, dhcp_server_ip);
    serial_printf("[DHCP] Sending Request for %u.%u.%u.%u\n",
                  dhcp_offered_ip & 0xFF, (dhcp_offered_ip >> 8) & 0xFF,
                  (dhcp_offered_ip >> 16) & 0xFF, (dhcp_offered_ip >> 24) & 0xFF);
    dhcp_send_raw(0, 68, 67, pkt, pkt_len);

    /* Wait for ACK */
    for (int i = 0; i < 100 && dhcp_state < 4; i++) {
        ne2000_poll_recv();
    }

    udp_callback = old_cb;
    return (dhcp_state == 4) ? 0 : -1;
}

/* ---- DNS Resolution ---- */

/* DNS header structure */
typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed)) dns_header_t;

/* DNS constants */
#define DNS_FLAG_RD      0x0100  /* Recursion Desired */
#define DNS_FLAG_QR      0x8000  /* Query Response */
#define DNS_TYPE_A       1
#define DNS_CLASS_IN     1

/* Static buffers for DNS callback */
static uint8_t dns_rx_buf[512];
static uint16_t dns_rx_len = 0;
static volatile int dns_rx_ready = 0;

/* Temporary UDP callback for DNS response */
static void dns_udp_cb(uint32_t src_ip, uint16_t src_port,
                        uint16_t dst_port,
                        const uint8_t* data, uint16_t len) {
    (void)src_ip;
    (void)dst_port;
    if (src_port == 53 && len > 0 && len <= 512) {
        memcpy(dns_rx_buf, data, len);
        dns_rx_len = len;
        dns_rx_ready = 1;
    }
}

/* Encode a hostname into DNS label format.
 * Input:  "www.example.com"
 * Output: "\x03www\x07example\x03com\x00"
 * Returns the encoded length. */
static int dns_encode_name(const char* hostname, uint8_t* out, int max_len) {
    int pos = 0;
    while (*hostname) {
        const char* dot = hostname;
        while (*dot && *dot != '.') dot++;
        int label_len = dot - hostname;
        if (label_len == 0) return -1;
        if (label_len > 63) return -1;
        if (pos + label_len + 1 > max_len) return -1;
        out[pos++] = (uint8_t)label_len;
        for (int i = 0; i < label_len; i++) {
            out[pos++] = (uint8_t)hostname[i];
        }
        if (*dot == '.') hostname = dot + 1;
        else break;
    }
    if (pos + 1 > max_len) return -1;
    out[pos++] = 0;
    return pos;
}

/* Resolve a hostname to an IP address via DNS query to 10.0.2.3:53.
 * Returns 0 on success, -1 on failure. */
int net_dns_query(const char* hostname, uint32_t* out_ip) {
    if (!hostname || !out_ip) return -1;

    uint32_t my_ip_val, gw, mask;
    net_get_config(&my_ip_val, &gw, &mask);
    if (my_ip_val == 0) return -1;

    /* Build DNS query packet */
    uint8_t dns_pkt[512];
    memset(dns_pkt, 0, sizeof(dns_pkt));

    dns_header_t* dns_hdr = (dns_header_t*)dns_pkt;
    dns_hdr->id = htons(0x1234);
    dns_hdr->flags = htons(DNS_FLAG_RD);
    dns_hdr->qdcount = htons(1);

    /* Encode the hostname as DNS labels */
    int qname_len = dns_encode_name(hostname, dns_pkt + sizeof(dns_header_t),
                                     sizeof(dns_pkt) - sizeof(dns_header_t) - 4);
    if (qname_len < 0) return -1;

    int qpos = sizeof(dns_header_t) + qname_len;
    dns_pkt[qpos++] = 0;
    dns_pkt[qpos++] = DNS_TYPE_A;
    dns_pkt[qpos++] = 0;
    dns_pkt[qpos++] = DNS_CLASS_IN;
    int dns_len = qpos;

    uint32_t dns_server = IP4(10, 0, 2, 3); /* QEMU's built-in DNS */
    uint16_t dns_port = 53;

    /* Save and replace UDP callback during DNS */
    udp_recv_callback_t old_cb = udp_callback;
    dns_rx_ready = 0;
    udp_callback = dns_udp_cb;

    int result = -1;

    /* First resolve ARP for DNS server if needed */
    const arp_entry_t* arp = net_arp_lookup(dns_server);
    if (!arp) {
        net_send_arp_request(dns_server);
        uint32_t arp_start = timer_get_ticks();
        while (timer_get_ticks() - arp_start < 10) {
            ne2000_poll_recv();
            arp = net_arp_lookup(dns_server);
            if (arp) break;
        }
        if (!arp) {
            udp_callback = old_cb;
            return -1;
        }
    }

    /* Build and send the DNS query packet */
    build_eth_header(arp->mac, ETHERTYPE_IP);

    uint8_t* udp_start = tx_frame + sizeof(eth_header_t) + 20;
    udp_header_t* udp = (udp_header_t*)udp_start;
    udp->src_port = htons(12345);
    udp->dst_port = htons(dns_port);
    udp->length = htons(sizeof(udp_header_t) + dns_len);
    udp->checksum = 0;
    memcpy(udp_start + sizeof(udp_header_t), dns_pkt, dns_len);

    uint16_t payload_len = sizeof(udp_header_t) + dns_len;

    ip_header_t* ip = (ip_header_t*)(tx_frame + sizeof(eth_header_t));
    ip->version_ihl = 0x45;
    ip->tos = 0;
    ip->total_length = htons(20 + payload_len);
    ip->identification = htons(ip_id_counter++);
    ip->flags_frag = htons(0x4000);
    ip->ttl = 64;
    ip->protocol = IP_PROTO_UDP;
    ip->src_ip = my_ip_val;
    ip->dst_ip = dns_server;
    ip->checksum = 0;
    ip->checksum = ip_checksum(ip, 20);

    uint16_t frame_len = sizeof(eth_header_t) + 20 + payload_len;

    ne2000_send(tx_frame, frame_len);
    stats.udp_sent++;

    /* Wait for response */
    uint32_t deadline = timer_get_ticks() + 50; /* ~1 second */
    while (timer_get_ticks() < deadline) {
        ne2000_poll_recv();
        if (dns_rx_ready) {
            if (dns_rx_len < (int)sizeof(dns_header_t)) break;

            dns_header_t* resp = (dns_header_t*)dns_rx_buf;
            uint16_t resp_flags = ntohs(resp->flags);
            uint16_t resp_ancount = ntohs(resp->ancount);

            if ((resp_flags & 0x000F) != 0) break;
            if (!(resp_flags & DNS_FLAG_QR)) break;

            /* Skip question section */
            int pos = sizeof(dns_header_t);
            while (pos < dns_rx_len) {
                uint8_t len = dns_rx_buf[pos];
                if (len == 0) { pos++; break; }
                if ((len & 0xC0) == 0xC0) { pos += 2; break; }
                pos += len + 1;
            }
            pos += 4; /* skip QTYPE + QCLASS */

            /* Process answer section (first A record only) */
            for (int a = 0; a < resp_ancount && a < 1; a++) {
                while (pos < dns_rx_len) {
                    uint8_t len = dns_rx_buf[pos];
                    if (len == 0) { pos++; break; }
                    if ((len & 0xC0) == 0xC0) { pos += 2; break; }
                    pos += len + 1;
                    if (pos >= dns_rx_len) break;
                }

                if (pos + 10 > dns_rx_len) break;
                uint16_t type = (dns_rx_buf[pos] << 8) | dns_rx_buf[pos+1];
                pos += 8; /* skip TYPE + CLASS + TTL */
                uint16_t rdlength = (dns_rx_buf[pos] << 8) | dns_rx_buf[pos+1];
                pos += 2;

                if (type == DNS_TYPE_A && rdlength == 4 && pos + 4 <= dns_rx_len) {
                    *out_ip = (uint32_t)dns_rx_buf[pos] |
                              ((uint32_t)dns_rx_buf[pos+1] << 8) |
                              ((uint32_t)dns_rx_buf[pos+2] << 16) |
                              ((uint32_t)dns_rx_buf[pos+3] << 24);
                    result = 0;
                    serial_printf("[DNS] Resolved %s -> %u.%u.%u.%u\n",
                                  hostname,
                                  *out_ip & 0xFF, (*out_ip >> 8) & 0xFF,
                                  (*out_ip >> 16) & 0xFF, (*out_ip >> 24) & 0xFF);
                }
                break;
            }
            break;
        }
        asm volatile("hlt");
    }

    udp_callback = old_cb;
    return result;
}

/* Check if a string is a valid IPv4 address (A.B.C.D) */
int net_is_valid_ip(const char* s) {
    int octets = 0;
    uint32_t val = 0;
    while (*s) {
        if (*s >= '0' && *s <= '9') {
            val = val * 10 + (*s - '0');
        } else if (*s == '.') {
            if (val > 255) return 0;
            octets++;
            val = 0;
        } else {
            return 0;
        }
        s++;
    }
    if (val > 255) return 0;
    return (octets == 3);
}

/* ---- Socket Abstraction Layer ---- */

static socket_t sockets[MAX_SOCKETS];

/* Route incoming UDP to sockets */
static void socket_udp_handler(uint32_t src_ip, uint16_t src_port,
                                uint16_t dst_port,
                                const uint8_t* data, uint16_t len) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (sockets[i].active && sockets[i].type == SOCK_DGRAM &&
            sockets[i].local_port == dst_port) {
            /* Copy into ring buffer */
            socket_t* s = &sockets[i];
            uint16_t avail = sizeof(s->rx_buf) - s->rx_count;
            uint16_t to_copy = len < avail ? len : avail;
            for (uint16_t j = 0; j < to_copy; j++) {
                s->rx_buf[s->rx_head] = data[j];
                s->rx_head = (s->rx_head + 1) % sizeof(s->rx_buf);
            }
            s->rx_count += to_copy;
            /* Store source info */
            s->remote_ip = src_ip;
            s->remote_port = src_port;
            break;
        }
    }
}

int sock_create(int type) {
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!sockets[i].active) {
            memset(&sockets[i], 0, sizeof(socket_t));
            sockets[i].type = type;
            sockets[i].active = 1;
            return i;
        }
    }
    return -1;
}

int sock_bind(int fd, uint16_t port) {
    if (fd < 0 || fd >= MAX_SOCKETS || !sockets[fd].active) return -1;
    sockets[fd].local_port = port;
    return 0;
}

int sock_connect(int fd, uint32_t ip, uint16_t port) {
    if (fd < 0 || fd >= MAX_SOCKETS || !sockets[fd].active) return -1;
    sockets[fd].remote_ip = ip;
    sockets[fd].remote_port = port;
    return 0;
}

int sock_send(int fd, const void* data, uint16_t len) {
    if (fd < 0 || fd >= MAX_SOCKETS || !sockets[fd].active) return -1;
    socket_t* s = &sockets[fd];
    if (s->type == SOCK_DGRAM) {
        return net_send_udp(s->remote_ip, s->remote_port, s->local_port, data, len);
    } else {
        return net_tcp_send(s->remote_ip, s->remote_port, data, len, TCP_PSH | TCP_ACK);
    }
}

int sock_recv(int fd, void* buf, uint16_t max_len, uint32_t timeout_ms) {
    if (fd < 0 || fd >= MAX_SOCKETS || !sockets[fd].active) return -1;
    socket_t* s = &sockets[fd];

    /* Wait for data with timeout */
    uint32_t deadline = timer_get_ticks() + (timeout_ms + 19) / 20;
    while (s->rx_count == 0) {
        if (timeout_ms > 0 && timer_get_ticks() >= deadline) return 0;
        ne2000_poll_recv();
        extern void enable_interrupts(void);
        asm volatile("hlt");
    }

    uint16_t to_read = s->rx_count < max_len ? s->rx_count : max_len;
    uint8_t* dst = (uint8_t*)buf;
    for (uint16_t i = 0; i < to_read; i++) {
        dst[i] = s->rx_buf[s->rx_tail];
        s->rx_tail = (s->rx_tail + 1) % sizeof(s->rx_buf);
    }
    s->rx_count -= to_read;
    return to_read;
}

int sock_listen(int fd) {
    if (fd < 0 || fd >= MAX_SOCKETS || !sockets[fd].active) return -1;
    if (sockets[fd].type == SOCK_STREAM) {
        net_tcp_listen(sockets[fd].local_port);
    }
    return 0;
}

int sock_accept(int fd, uint32_t* out_ip, uint16_t* out_port) {
    (void)fd; (void)out_ip; (void)out_port;
    /* TCP accept not yet implemented (would need connection queue) */
    return -1;
}

void sock_close(int fd) {
    if (fd < 0 || fd >= MAX_SOCKETS || !sockets[fd].active) return;
    if (sockets[fd].type == SOCK_STREAM) {
        net_tcp_close(sockets[fd].remote_ip, sockets[fd].remote_port);
    }
    sockets[fd].active = 0;
}

/* Install socket UDP handler (call from net_init or shell) */
void net_socket_init(void) {
    memset(sockets, 0, sizeof(sockets));
    net_set_udp_callback(socket_udp_handler);
    serial_printf("[NET] Socket layer initialized (%d sockets)\n", MAX_SOCKETS);
}

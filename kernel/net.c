#include "../include/net.h"
#include "../include/ne2000.h"
#include "../include/stdio.h"
#include "../include/string.h"

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

/*
 * httpclient.c - TinyOS HTTP 1.0 Client
 *
 * Sends an HTTP GET request to a remote server and displays the response.
 * Uses the TCP layer in net.c.
 *
 * Usage: In shell, type "http-get <host> [port] [path]"
 * Example: http-get httpbin.org 80 /get
 *          http-get 10.0.2.2 8080 /api/data
 */

#include "../include/httpclient.h"
#include "../include/net.h"
#include "../include/ne2000.h"
#include "../include/stdio.h"
#include "../include/string.h"
#include "../include/serial.h"
#include "../include/timer.h"
#include "../include/io.h"

#define HTTP_RESP_BUF_SIZE 4096

/* Receive buffer for HTTP response data */
static char http_resp_buf[HTTP_RESP_BUF_SIZE];
static volatile uint16_t http_resp_len = 0;

/**
 * TCP receive callback — called when TCP data arrives on our connection.
 * Accumulates received data into the response buffer.
 */
static void http_recv_callback(uint32_t src_ip, uint16_t src_port,
                                const uint8_t* data, uint16_t len) {
    (void)src_ip;
    (void)src_port;

    uint16_t avail = HTTP_RESP_BUF_SIZE - http_resp_len;
    uint16_t to_copy = len < avail ? len : avail;
    memcpy(http_resp_buf + http_resp_len, data, to_copy);
    http_resp_len += to_copy;

    serial_printf("[HTTP-C] Received %u bytes (total %u)\n", len, http_resp_len);
}

int http_get(const char* host, uint16_t port, const char* path) {
    if (!host || !path) return -1;

    uint32_t ip;

    /* Resolve host: try as IP first, then DNS */
    if (net_parse_ip(host, &ip) != 0) {
        /* Not an IP, try DNS resolution */
        printf("[HTTP] Resolving %s...\n", host);
        if (net_dns_query(host, &ip) < 0) {
            printf("[HTTP] Could not resolve: %s\n", host);
            return -1;
        }
        printf("[HTTP] Resolved %s -> %u.%u.%u.%u\n", host,
               ip & 0xFF, (ip >> 8) & 0xFF,
               (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
    }

    /* Save current TCP callback and install ours */
    tcp_recv_callback_t old_cb = net_get_tcp_callback();
    http_resp_len = 0;
    net_set_tcp_callback(http_recv_callback);

    /* Initiate TCP connection */
    printf("[HTTP] Connecting to %u.%u.%u.%u:%u...\n",
           ip & 0xFF, (ip >> 8) & 0xFF,
           (ip >> 16) & 0xFF, (ip >> 24) & 0xFF, port);

    if (net_tcp_connect(ip, port) < 0) {
        printf("[HTTP] Connection failed\n");
        net_set_tcp_callback(old_cb);
        return -1;
    }

    /* Send EOI for keyboard IRQ1 since we're in shell handler context */
    outb(0x20, 0x20);  /* EOI to master PIC */

    /* Wait for connection to establish (poll with timeout) */
    uint32_t deadline = timer_get_ticks() + 150;  /* ~3 seconds at 50 Hz */
    while (timer_get_ticks() < deadline) {
        ne2000_poll_recv();
        if (net_tcp_is_connected(ip, port)) break;
        asm volatile("hlt");
    }

    if (!net_tcp_is_connected(ip, port)) {
        printf("[HTTP] Connection timeout to %u.%u.%u.%u:%u\n",
               ip & 0xFF, (ip >> 8) & 0xFF,
               (ip >> 16) & 0xFF, (ip >> 24) & 0xFF, port);
        net_set_tcp_callback(old_cb);
        return -1;
    }

    printf("[HTTP] Connected. Sending GET request...\n");

    /* Build and send HTTP GET request (HTTP/1.0 with Connection: close) */
    char request[512];
    int req_len = sprintf(request,
        "GET %s HTTP/1.0\r\n"
        "Host: %s\r\n"
        "Connection: close\r\n"
        "\r\n",
        path, host);

    if (req_len <= 0) {
        printf("[HTTP] Failed to build request\n");
        net_tcp_close(ip, port);
        net_set_tcp_callback(old_cb);
        return -1;
    }

    net_tcp_send(ip, port, request, (uint16_t)req_len, TCP_PSH | TCP_ACK);

    /* Wait for response data (poll with timeout) */
    deadline = timer_get_ticks() + 250;  /* ~5 seconds */
    while (timer_get_ticks() < deadline) {
        ne2000_poll_recv();
        if (http_resp_len > 0) break;  /* Got at least some data */
        asm volatile("hlt");
    }

    if (http_resp_len == 0) {
        printf("[HTTP] No response received within timeout\n");
        net_tcp_close(ip, port);
        net_set_tcp_callback(old_cb);
        return -1;
    }

    /* Wait a bit more to receive additional data (connection may close) */
    deadline = timer_get_ticks() + 100;  /* ~2 more seconds */
    do {
        ne2000_poll_recv();
        asm volatile("hlt");
    } while (timer_get_ticks() < deadline && http_resp_len < HTTP_RESP_BUF_SIZE);

    /* Display the HTTP response */
    printf("[HTTP] Response (%u bytes):\n", http_resp_len);
    http_resp_buf[http_resp_len < HTTP_RESP_BUF_SIZE ? http_resp_len : HTTP_RESP_BUF_SIZE - 1] = '\0';
    printf("%s", http_resp_buf);
    if (http_resp_len > 0 && http_resp_buf[http_resp_len - 1] != '\n') {
        printf("\n");
    }

    /* Close the TCP connection */
    net_tcp_close(ip, port);

    /* Restore old TCP callback */
    net_set_tcp_callback(old_cb);

    return 0;
}
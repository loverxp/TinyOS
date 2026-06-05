/*
 * webserver.c - TinyOS HTTP 1.0 Server
 *
 * Serves a simple status/info page on port 80.
 * Uses the TCP layer in net.c.
 *
 * Usage: In shell, type "webserver" to start, "webserver stop" to stop.
 */

#include "../include/net.h"
#include "../include/stdio.h"
#include "../include/string.h"
#include "../include/serial.h"

static int webserver_running = 0;

/* The HTML page we serve */
static const char* html_template =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!DOCTYPE html>\n"
    "<html><head><meta charset=\"utf-8\">\n"
    "<title>TinyOS WebServer</title>\n"
    "<style>\n"
    "  body { font-family: monospace; background: #1a1a2e; color: #eee; "
    "padding: 2em; }\n"
    "  h1 { color: #00d4aa; }\n"
    "  .info { background: #16213e; padding: 1em; border-radius: 8px; }\n"
    "  .label { color: #888; }\n"
    "  .value { color: #00d4aa; }\n"
    "</style></head><body>\n"
    "<h1>TinyOS WebServer</h1>\n"
    "<div class=\"info\">\n"
    "  <p><span class=\"label\">Kernel:</span> <span class=\"value\">TinyOS</span></p>\n"
    "  <p><span class=\"label\">Arch:</span> <span class=\"value\">i386 (x86)</span></p>\n"
    "  <p><span class=\"label\">Status:</span> <span class=\"value\">Running</span></p>\n"
    "</div>\n"
    "<p style=\"color:#666;margin-top:2em;\">Powered by TinyOS — "
    "<em>a simple OS kernel running in QEMU</em></p>\n"
    "</body></html>\n";

/* TCP receive callback — called when HTTP request data arrives */
static void webserver_on_data(uint32_t src_ip, uint16_t src_port,
                               const uint8_t* data, uint16_t len) {
    if (!webserver_running) return;

    serial_printf("[HTTP] Request from %u.%u.%u.%u:%u (%u bytes)\n",
                  src_ip & 0xFF, (src_ip >> 8) & 0xFF,
                  (src_ip >> 16) & 0xFF, (src_ip >> 24) & 0xFF,
                  src_port, len);

    /* Print first line of HTTP request for debugging */
    for (uint16_t i = 0; i < len && i < 80; i++) {
        if (data[i] == '\r' || data[i] == '\n') break;
        serial_printf("%c", data[i]);
    }
    serial_printf("\n");

    /* Send HTTP response */
    char response[1600];
    int n = sprintf(response, "%s", html_template);
    if (n > 0) {
        serial_printf("[HTTP] Sending %d bytes response\n", n);
        net_tcp_send(src_ip, src_port, response, (uint16_t)n, TCP_PSH);
    }

    /* Close the connection */
    net_tcp_close(src_ip, src_port);
}

/* ---- Shell command interface ---- */

void webserver_start(void) {
    if (webserver_running) {
        printf("WebServer is already running on port 80.\n");
        return;
    }

    net_tcp_listen(80);
    net_set_tcp_callback(webserver_on_data);
    webserver_running = 1;

    printf("WebServer started on port 80.\n");
    printf("Connect to http://localhost:8088/ (QEMU hostfwd)\n");
}

void webserver_stop(void) {
    if (!webserver_running) {
        printf("WebServer is not running.\n");
        return;
    }

    webserver_running = 0;
    net_set_tcp_callback(NULL);
    net_tcp_listen(0);  /* Stop listening */

    printf("WebServer stopped.\n");
}

int webserver_is_running(void) {
    return webserver_running;
}
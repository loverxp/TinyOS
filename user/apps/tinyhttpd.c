/* tinyhttpd.c - Minimal HTTP server for TinyOS
 * Demonstrates socket syscalls (39-46).
 * Listens on port 80, serves a simple HTML page.
 */

#include <syscall.h>
#include <stdio.h>
#include <string.h>

#define PORT 80
#define BUF_SIZE 512

static const char http_response[] =
    "HTTP/1.0 200 OK\r\n"
    "Content-Type: text/html\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!DOCTYPE html>\n"
    "<html><head><title>TinyOS HTTP Server</title></head>\n"
    "<body>\n"
    "<h1>Hello from TinyOS!</h1>\n"
    "<p>This page is served by tinyhttpd running in Ring 3.</p>\n"
    "<p>Socket syscalls 39-46 working correctly.</p>\n"
    "</body></html>\n";

static const char not_found[] =
    "HTTP/1.0 404 Not Found\r\n"
    "Content-Type: text/plain\r\n"
    "\r\n"
    "404 Not Found\n";

int main(void) {
    printf("tinyhttpd: starting HTTP server on port %d\n", PORT);

    /* Create TCP socket */
    int sock = socket_create(SOCK_TYPE_TCP);
    if (sock < 0) {
        printf("tinyhttpd: socket_create failed (%d)\n", sock);
        return 1;
    }
    printf("tinyhttpd: socket created (fd=%d)\n", sock);

    /* Bind to port */
    int ret = socket_bind(sock, PORT);
    if (ret < 0) {
        printf("tinyhttpd: bind to port %d failed (%d)\n", PORT, ret);
        socket_close(sock);
        return 1;
    }
    printf("tinyhttpd: bound to port %d\n", PORT);

    /* Listen */
    ret = socket_listen(sock);
    if (ret < 0) {
        printf("tinyhttpd: listen failed (%d)\n", ret);
        socket_close(sock);
        return 1;
    }
    printf("tinyhttpd: listening... (use http://<ip>:%d/)\n", PORT);

    /* Accept loop */
    int served = 0;
    while (served < 10) {
        net_addr_t client;
        int client_fd = socket_accept(sock, &client);
        if (client_fd < 0) {
            /* No connection yet, spin and retry */
            continue;
        }

        printf("tinyhttpd: connection from %d.%d.%d.%d:%d\n",
               client.ip & 0xFF, (client.ip >> 8) & 0xFF,
               (client.ip >> 16) & 0xFF, (client.ip >> 24) & 0xFF,
               client.port);

        /* Read request (non-blocking with timeout) */
        char buf[BUF_SIZE];
        int n = socket_recv(client_fd, buf, BUF_SIZE - 1);
        if (n > 0) {
            buf[n] = '\0';
            /* Parse first line: GET /path HTTP/1.x */
            printf("tinyhttpd: request (%d bytes)\n", n);

            /* Check for GET / */
            const char* response = http_response;
            int resp_len = sizeof(http_response) - 1;

            if (n > 4 && buf[0] == 'G' && buf[1] == 'E' && buf[2] == 'T') {
                /* Check path */
                char* path_start = buf + 4;
                if (*path_start != '/' || (*(path_start + 1) != ' ' && *(path_start + 1) != '?')) {
                    response = not_found;
                    resp_len = sizeof(not_found) - 1;
                }
            }

            socket_send(client_fd, response, (uint16_t)resp_len);
            served++;
            printf("tinyhttpd: served %d/%d\n", served, 10);
        }

        socket_close(client_fd);
    }

    printf("tinyhttpd: served %d requests, shutting down\n", served);
    socket_close(sock);
    return 0;
}

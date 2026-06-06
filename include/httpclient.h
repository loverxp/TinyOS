#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H

#include "types.h"

/**
 * http_get - Send an HTTP GET request to a remote server and display the response.
 * @host:  Hostname (e.g. "httpbin.org") or IPv4 address (e.g. "10.0.2.2")
 * @port:  TCP port (default 80)
 * @path:  Request path (e.g. "/get" or "/index.html")
 * 
 * Returns 0 on success, -1 on failure.
 */
int http_get(const char* host, uint16_t port, const char* path);

#endif /* HTTPCLIENT_H */
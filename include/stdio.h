#ifndef STDIO_H
#define STDIO_H

#include "types.h"

// Format to string buffer
int sprintf(char* buf, const char* fmt, ...);

// Print formatted string to VGA display
void printf(const char* fmt, ...);

// Print a single character
void putchar(char c);

// Print a string with newline
void puts(const char* s);

// Print to serial port (for debugging)
void serial_printf(const char* fmt, ...);

/* Hook for redirecting printf output (e.g. to a pipe).
 * Set to a function to capture all printf output, or NULL for normal output. */
typedef void (*printf_pipe_hook_t)(const char* str, uint32_t len);
extern printf_pipe_hook_t printf_pipe_redirect;

#endif // STDIO_H

/* stdio.h - Standard I/O functions */

#ifndef STDIO_H
#define STDIO_H

#include <stddef.h>
#include <stdarg.h>

/* Standard streams (mapped to console) */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Write string to console */
int puts(const char* s);

/* Write character to console */
int putchar(int c);

/* Write formatted string to console */
int printf(const char* fmt, ...);

/* Format string into buffer */
int sprintf(char* buf, const char* fmt, ...);

/* Format string into buffer with size limit */
int snprintf(char* buf, size_t size, const char* fmt, ...);

/* Read formatted input from keyboard */
int scanf(const char* fmt, ...);

/* Internal: format string with va_list */
int vsprintf(char* buf, const char* fmt, va_list args);
int vsnprintf(char* buf, size_t size, const char* fmt, va_list args);

#endif /* STDIO_H */

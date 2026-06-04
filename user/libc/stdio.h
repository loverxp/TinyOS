/* stdio.h - Standard I/O functions */

#ifndef STDIO_H
#define STDIO_H

/* Write string to console */
int puts(const char* s);

/* Write character to console */
int putchar(int c);

/* Write formatted string to console */
int printf(const char* fmt, ...);

/* Format string into buffer */
int sprintf(char* buf, const char* fmt, ...);

#endif /* STDIO_H */
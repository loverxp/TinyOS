#ifndef STDIO_H
#define STDIO_H

#include "types.h"

// Print formatted string to VGA display
void printf(const char* fmt, ...);

// Print a single character
void putchar(char c);

// Print a string with newline
void puts(const char* s);

// Print to serial port (for debugging)
void serial_printf(const char* fmt, ...);

#endif // STDIO_H

#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

/* Callback type for received characters */
typedef void (*serial_char_callback_t)(char);

/* Initialize serial port RX interrupts */
void serial_init(void);

/* Send a single character to serial port */
void serial_putchar(char c);

/* Send a null-terminated string to serial port */
void serial_writestring(const char* s);

/* Register a callback for received characters (set to NULL to disable) */
void serial_register_callback(serial_char_callback_t callback);

/* IRQ 4 handler - called from interrupt context */
void serial_handler(void);

#endif
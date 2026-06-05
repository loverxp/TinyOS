#include "../include/serial.h"
#include "../include/io.h"

/* COM1 port addresses */
#define COM1_DATA    0x3F8
#define COM1_IER     0x3F9   /* Interrupt Enable Register */
#define COM1_IIR     0x3FA   /* Interrupt ID Register */
#define COM1_LCR     0x3FB   /* Line Control Register */
#define COM1_LSR     0x3FD   /* Line Status Register */
#define COM1_DLL     0x3F8   /* Divisor Latch Low (DLAB=1) */
#define COM1_DLM     0x3F9   /* Divisor Latch High (DLAB=1) */

/* Line Status Register bits */
#define LSR_DR       0x01    /* Data Ready - byte available to read */
#define LSR_THRE     0x20    /* Transmitter Holding Register Empty */

/* RX ring buffer (backup storage when callback is NULL/busy) */
#define RX_BUF_SIZE 256
static volatile char rx_buf[RX_BUF_SIZE];
static volatile int rx_head = 0;
static volatile int rx_tail = 0;

/* Registered callback for received characters */
static serial_char_callback_t rx_callback = NULL;

/* Send a single character. Handles LF -> CRLF conversion. */
void serial_putchar(char c) {
    /* Wait for transmitter to be ready */
    while ((inb(COM1_LSR) & LSR_THRE) == 0);
    outb(COM1_DATA, c);
    /* Most serial terminals expect CR+LF for newline */
    if (c == '\n') {
        while ((inb(COM1_LSR) & LSR_THRE) == 0);
        outb(COM1_DATA, '\r');
    }
}

void serial_writestring(const char* s) {
    while (*s) serial_putchar(*s++);
}

void serial_register_callback(serial_char_callback_t callback) {
    rx_callback = callback;
}

/* IRQ 4 handler — called from interrupt context.
 *
 * COM1 interrupt triggers when a byte arrives (IER bit 0 set).
 * Read the byte, optionally call the registered callback,
 * and store in ring buffer as a safety net. */
void serial_handler(void) {
    /* Check if Data Ready (LSR bit 0) */
    if (!(inb(COM1_LSR) & LSR_DR)) return;

    char c = inb(COM1_DATA);

    /* Store in ring buffer (always, for safety) */
    int next = (rx_head + 1) % RX_BUF_SIZE;
    if (next != rx_tail) {
        rx_buf[rx_head] = c;
        rx_head = next;
    }

    /* Call registered callback if present */
    if (rx_callback) {
        rx_callback(c);
    }
}

/* Initialize COM1 for RX interrupts.
 *
 * Assumes COM1 has already been configured for basic serial I/O
 * (baud rate, 8N1) by kernel_main's serial init sequence.
 * We just need to enable the Received Data Available interrupt. */
void serial_init(void) {
    /* IER bit 0 = Enable Received Data Available interrupt */
    outb(COM1_IER, 0x01);
}
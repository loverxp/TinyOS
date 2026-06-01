#include "../include/timer.h"
#include "../include/io.h"

// PIT ports
#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40

// PIT frequency
#define PIT_FREQUENCY 1193182

static volatile uint32_t timer_ticks = 0;

// Simple debug output to serial port
static void serial_write(char c) {
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, c);
}

static void serial_string(const char* s) {
    while (*s) serial_write(*s++);
}

static void serial_hex(uint32_t n) {
    char hex[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        serial_write(hex[(n >> i) & 0xF]);
    }
}

void timer_handler(void) {
    timer_ticks++;
}

uint32_t timer_get_ticks(void) {
    return timer_ticks;
}

void timer_initialize(uint32_t frequency) {
    uint32_t divisor = PIT_FREQUENCY / frequency;
    
    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));
    
    serial_string("[TIMER] Initialized at ");
    serial_hex(frequency);
    serial_string(" Hz\n");
}

#include "../include/timer.h"
#include "../include/io.h"

// PIT ports
#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40

// PIT frequency
#define PIT_FREQUENCY 1193182

static volatile uint32_t timer_ticks = 0;
static uint32_t frequency_hz = 0;
static timer_second_callback_t second_callback = NULL;
static void (*tick_callback)(void) = NULL;

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

void timer_register_second_callback(timer_second_callback_t callback) {
    second_callback = callback;
}

void timer_register_tick_callback(void (*callback)(void)) {
    tick_callback = callback;
}

void timer_handler(void) {
    timer_ticks++;

    // Fire tick callback every tick (for game loops)
    if (tick_callback) {
        tick_callback();
    }

    // Fire second callback every 'frequency_hz' ticks (1 second)
    if (second_callback && (timer_ticks % frequency_hz == 0)) {
        second_callback();
    }
}

uint32_t timer_get_ticks(void) {
    return timer_ticks;
}

void timer_initialize(uint32_t frequency) {
    uint32_t divisor = PIT_FREQUENCY / frequency;
    frequency_hz = frequency;

    outb(PIT_COMMAND_PORT, 0x36);
    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    serial_string("[TIMER] Initialized at ");
    serial_hex(frequency);
    serial_string(" Hz\n");
}
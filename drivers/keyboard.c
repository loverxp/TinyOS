#include "../include/keyboard.h"
#include "../include/io.h"
#include "../include/vga.h"

// Keyboard ports
#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64

// Keyboard buffer
#define KEYBOARD_BUFFER_SIZE 256

static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile size_t buffer_head = 0;
static volatile size_t buffer_tail = 0;

// US QWERTY keyboard scancode to ASCII mapping (set 1)
static const char scancode_to_ascii[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,   '\\\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static volatile int shift_pressed = 0;

// Simple debug output to serial port
static void serial_write(char c) {
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, c);
}

static void serial_string(const char* s) {
    while (*s) serial_write(*s++);
}

static void serial_hex(uint8_t n) {
    char hex[] = "0123456789ABCDEF";
    serial_write(hex[n >> 4]);
    serial_write(hex[n & 0xF]);
}

int keyboard_has_input(void) {
    return buffer_head != buffer_tail;
}

char keyboard_read_char(void) {
    while (buffer_head == buffer_tail) {
        // Wait for input
    }
    char c = keyboard_buffer[buffer_head];
    buffer_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

void keyboard_handler(void) {
    // Read scancode from keyboard
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    serial_string("[IRQ1] scancode=0x");
    serial_hex(scancode);
    serial_string("\n");
    
    // Check if key release (bit 7 set)
    if (scancode & 0x80) {
        scancode &= 0x7F;
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 0;
        }
    } else {
        // Key press
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = 1;
        } else if (scancode < sizeof(scancode_to_ascii)) {
            char c = scancode_to_ascii[scancode];
            if (c != 0) {
                size_t next_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
                if (next_tail != buffer_head) {
                    keyboard_buffer[buffer_tail] = c;
                    buffer_tail = next_tail;
                    serial_string("[IRQ1] buffered char='");
                    serial_write(c);
                    serial_string("'\n");
                }
            }
        }
    }
}

void keyboard_initialize(void) {
    // Empty the keyboard buffer
    while (inb(KEYBOARD_STATUS_PORT) & 1) {
        inb(KEYBOARD_DATA_PORT);
    }
    serial_string("[KBD] Keyboard initialized\n");
}

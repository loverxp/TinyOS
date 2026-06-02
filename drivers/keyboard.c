#include "../include/keyboard.h"
#include "../include/io.h"
#include "../include/vga.h"

// Keyboard ports
#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64

static keyboard_char_callback_t char_callback = NULL;

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

void keyboard_register_char_callback(keyboard_char_callback_t callback) {
    char_callback = callback;
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
                serial_string("[IRQ1] char='");
                serial_write(c);
                serial_string("'\n");

                // Event-driven: call callback directly
                if (char_callback) {
                    char_callback(c);
                }

                // Press E key to trigger exception demo
                if (c == 'e') {
                    serial_string("[DEMO] Triggering int $0 (Division By Zero)...\n");
                    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
                    vga_writestring("\n>>> Triggering int 0 (Division By Zero) <<<");
                    asm volatile("int $0");
                    vga_writestring("\n>>> Returned from exception, continuing...\n");
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
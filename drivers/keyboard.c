#include "../include/keyboard.h"
#include "../include/io.h"
#include "../include/vga.h"

// Keyboard ports
#define KEYBOARD_DATA_PORT    0x60
#define KEYBOARD_STATUS_PORT  0x64

static keyboard_char_callback_t char_callback = NULL;
static keyboard_raw_callback_t raw_callback = NULL;

// Track 0xE0 extended prefix for arrow keys etc.
static volatile int extended_prefix = 0;

// Simple keyboard buffer for non-blocking reads from user mode
#define KBD_BUF_SIZE 16
static volatile uint8_t kbd_buffer[KBD_BUF_SIZE];
static volatile int kbd_buf_head = 0;
static volatile int kbd_buf_tail = 0;

static void kbd_buf_push(uint8_t scancode, uint8_t extended) {
    int next = (kbd_buf_head + 1) % KBD_BUF_SIZE;
    if (next == kbd_buf_tail) return; // buffer full
    kbd_buffer[kbd_buf_head] = scancode | (extended ? 0x80 : 0);
    kbd_buf_head = next;
}

uint32_t keyboard_read_key(void) {
    disable_interrupts();
    if (kbd_buf_head == kbd_buf_tail) {
        enable_interrupts();
        return 0; // no key available
    }
    uint8_t key = kbd_buffer[kbd_buf_tail];
    kbd_buf_tail = (kbd_buf_tail + 1) % KBD_BUF_SIZE;
    enable_interrupts();
    return key;
}

void keyboard_clear_buffer(void) {
    disable_interrupts();
    kbd_buf_head = 0;
    kbd_buf_tail = 0;
    enable_interrupts();
}

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

void keyboard_register_raw_callback(keyboard_raw_callback_t callback) {
    raw_callback = callback;
    serial_string("[KBD] Raw callback registered: ");
    serial_hex((uint32_t)callback >> 16);
    serial_hex((uint32_t)callback & 0xFFFF);
    serial_string("\n");
}

void keyboard_handler(void) {
    // Read scancode from keyboard
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    serial_string("[KBD] sc=0x");
    serial_hex(scancode);
    serial_string(" ext=");
    serial_hex(extended_prefix ? 1 : 0);
    serial_string("\n");

    // Handle 0xE0 prefix (extended scancodes: arrow keys, etc.)
    if (scancode == 0xE0) {
        extended_prefix = 1;
        return;
    }

    // Push to keyboard buffer for user mode non-blocking reads
    if (!(scancode & 0x80)) {
        kbd_buf_push(scancode, extended_prefix);
    }

    // Call raw callback if registered (for key press events only)
    if (raw_callback && !(scancode & 0x80)) {
        serial_string("[KBD] Calling raw callback\n");
        raw_callback(scancode, extended_prefix);
    }

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
                // Event-driven: call callback directly
                if (char_callback) {
                    char_callback(c);
                }
            }
        }
    }
    extended_prefix = 0;
}

void keyboard_initialize(void) {
    // Empty the keyboard buffer
    while (inb(KEYBOARD_STATUS_PORT) & 1) {
        inb(KEYBOARD_DATA_PORT);
    }
    serial_string("[KBD] Keyboard initialized\n");
}
// mouse.c - PS/2 Mouse driver (IRQ12)

#include "../include/mouse.h"
#include "../include/io.h"
#include "../include/keyboard.h"  // For send_keyboard_command
#include "../include/stdio.h"
#include "../include/framebuf.h"

// Global mouse state
int mouse_x = 0;
int mouse_y = 0;
uint8_t mouse_buttons = 0;

static mouse_event_cb event_callback = NULL;

// PS/2 mouse expects the following packet format from device:
// Byte 0: YOV XOV YS XS 1 M R L  (status)
// Byte 1: X movement
// Byte 2: Y movement (negative = up)
// (For scroll wheel, there are additional bytes)

// Wait for PS/2 controller input buffer to be ready
static int mouse_wait_input(void) {
    int timeout = 100000;
    while (timeout--) {
        if (inb(0x64) & 0x02) return 1;  // Input buffer empty
    }
    return 0;
}

// Wait for PS/2 controller output buffer to be ready
static int mouse_wait_output(void) {
    int timeout = 100000;
    while (timeout--) {
        if (inb(0x64) & 0x01) return 1;  // Output buffer full (data ready)
    }
    return 0;
}

// Write to PS/2 controller command port
static void mouse_write_cmd(uint8_t cmd) {
    mouse_wait_input();
    outb(0x64, cmd);
}

// Write to PS/2 data port
static void mouse_write_data(uint8_t data) {
    mouse_wait_input();
    outb(0x60, data);
}

// Read from PS/2 data port
static uint8_t mouse_read_data(void) {
    mouse_wait_output();
    return inb(0x60);
}

// PS/2 mouse initialization sequence:
static void mouse_command(uint8_t cmd) {
    mouse_write_cmd(0xD4);  // Send to mouse
    mouse_write_data(cmd);
    mouse_read_data();  // ACK (0xFA)
}

void mouse_init(void) {
    printf("[MOUSE] Initializing PS/2 mouse...\n");

    // Enable auxiliary device (mouse)
    mouse_write_cmd(0xA8);
    mouse_read_data();  // ACK

    // Get current command byte from controller
    mouse_write_cmd(0x20);
    uint8_t status = mouse_read_data();

    // Enable IRQ12 (bit 1) and enable mouse clock (bit 5)
    status |= 0x02;
    mouse_write_cmd(0x60);
    mouse_write_data(status);
    mouse_read_data();  // ACK

    // Set mouse defaults
    mouse_command(0xF6);

    // Enable mouse data reporting
    mouse_command(0xF4);

    // Set sample rate (optional)
    // mouse_command(0xF3);  // Set sample rate
    // mouse_command(100);

    // Reset mouse position
    mouse_x = fb.width / 2;
    mouse_y = fb.height / 2;
    mouse_buttons = 0;

    printf("[MOUSE] OK (IRQ12)\n");
}

// Mouse IRQ12 handler - reads 3-byte packet from PS/2 data port
void mouse_handler(void) {
    static uint8_t packet[3];
    static int packet_idx = 0;

    uint8_t data = inb(0x60);

    packet[packet_idx++] = data;

    if (packet_idx == 1) {
        // Byte 0 must have bit 3 set (always 1 for sync)
        if (!(packet[0] & 0x08)) {
            packet_idx = 0;  // Out of sync, reset
        }
    }

    if (packet_idx == 3) {
        packet_idx = 0;

        // Parse packet
        int dx = (int)(int8_t)packet[1];
        int dy = -(int)(int8_t)packet[2];  // PS/2 Y+ = up, screen Y+ = down → negate
        uint8_t btns = packet[0] & 0x07;

        // Overflow check - ignore if overflow bits are set
        if (!(packet[0] & 0x80)) {
            // Update absolute position with bounds clamping
            mouse_x += dx;
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= (int)fb.width) mouse_x = fb.width - 1;
        }

        if (!(packet[0] & 0x40)) {
            mouse_y += dy;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= (int)fb.height) mouse_y = fb.height - 1;
        }

        mouse_buttons = btns;

        // Notify callback
        if (event_callback) {
            event_callback(mouse_x, mouse_y, mouse_buttons);
        }
    }
}

void mouse_register_callback(mouse_event_cb cb) {
    event_callback = cb;
}

int mouse_get_x(void)            { return mouse_x; }
int mouse_get_y(void)            { return mouse_y; }
uint8_t mouse_get_buttons(void)  { return mouse_buttons; }
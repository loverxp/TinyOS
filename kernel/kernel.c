#include "../include/vga.h"
#include "../include/io.h"
#include "../include/interrupts.h"
#include "../include/keyboard.h"
#include "../include/timer.h"

extern void gdt_init(void);

static uint32_t seconds = 0;

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

// Called every second from timer interrupt
void on_timer_second(void) {
    seconds++;
    serial_string("[TIMER] ");
    serial_hex(seconds);
    serial_string("s\n");

    // Save cursor, update status line, restore cursor
    size_t save_row = vga_get_cursor_row();
    size_t save_col = vga_get_cursor_column();
    vga_set_cursor(VGA_HEIGHT - 1, 0);
    vga_writestring("Timer: ");
    vga_write_dec(seconds);
    vga_writestring("s                                          ");
    vga_set_cursor(save_row, save_col);
}

// Called on each key press from keyboard interrupt
void on_keyboard_char(char c) {
    if (c == '\n') {
        vga_putchar('\n');
        vga_writestring("> ");
    } else if (c == '\b') {
        vga_putchar('\b');
    } else if (c >= 32 && c < 127) {
        vga_putchar(c);
    }
}

void kernel_main(void) {
    // Initialize serial port (COM1)
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x80);
    outb(0x3F8, 0x01);
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x03);
    outb(0x3FA, 0xC7);
    outb(0x3FC, 0x0B);

    serial_string("=== TinyOS Debug ===\n");

    gdt_init();
    serial_string("[OK] GDT\n");

    vga_initialize();
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_writestring("TinyOS v0.1 - Kernel Loaded\n");
    vga_writestring("==========================\n\n");
    serial_string("[OK] VGA\n");

    idt_initialize();
    vga_writestring("[OK] IDT initialized\n");
    serial_string("[OK] IDT\n");

    pic_initialize();
    vga_writestring("[OK] PIC initialized\n");
    serial_string("[OK] PIC\n");

    timer_initialize(50);
    timer_register_second_callback(on_timer_second);
    register_interrupt_handler(32, timer_handler);
    pic_unmask_irq(0);
    vga_writestring("[OK] Timer initialized (50 Hz)\n");
    serial_string("[OK] Timer\n");

    keyboard_initialize();
    keyboard_register_char_callback(on_keyboard_char);
    register_interrupt_handler(33, keyboard_handler);
    pic_unmask_irq(1);
    vga_writestring("[OK] Keyboard initialized\n");
    serial_string("[OK] Keyboard\n");

    enable_interrupts();
    vga_writestring("[OK] Interrupts enabled\n\n");
    serial_string("[OK] Interrupts enabled\n");

    vga_writestring("Type something. Press 'E' for exception demo.\n");
    vga_writestring("> ");
    serial_string("Ready, entering main loop...\n");

    // Event-driven main loop: just wait for interrupts
    while (1) {
        halt();
    }
}
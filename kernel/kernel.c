#include "../include/vga.h"
#include "../include/io.h"
#include "../include/interrupts.h"
#include "../include/keyboard.h"
#include "../include/timer.h"

extern void gdt_init(void);

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

void kernel_main(void) {
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
    register_interrupt_handler(32, timer_handler);
    pic_unmask_irq(0);
    vga_writestring("[OK] Timer initialized (50 Hz)\n");
    serial_string("[OK] Timer\n");

    keyboard_initialize();
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

    uint32_t last_ticks = 0;
    uint32_t seconds = 0;

    while (1) {
        uint32_t ticks = timer_get_ticks();
        if (ticks != last_ticks) {
            last_ticks = ticks;
            if (ticks % 50 == 0) {
                seconds++;
                serial_string("[TIMER] ");
                serial_hex(seconds);
                serial_string("s\n");
            }
        }

        if (keyboard_has_input()) {
            char c = keyboard_read_char();
            if (c == '\n') {
                vga_putchar('\n');
                vga_writestring("> ");
            } else if (c == '\b') {
                vga_putchar('\b');
            } else if (c >= 32 && c < 127) {
                vga_putchar(c);
            }
        }

        halt();
    }
}
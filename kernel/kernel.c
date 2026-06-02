#include "../include/vga.h"
#include "../include/io.h"
#include "../include/interrupts.h"
#include "../include/keyboard.h"

extern void gdt_init(void);

void kernel_main(void) {
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x80);
    outb(0x3F8, 0x01);
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x03);
    outb(0x3FA, 0xC7);
    outb(0x3FC, 0x0B);

    gdt_init();

    vga_initialize();
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    vga_writestring("TinyOS v0.1 - Kernel Loaded Successfully\n");
    vga_writestring("========================================\n\n");

    vga_writestring("[OK] GDT initialized\n");
    vga_writestring("[OK] VGA initialized\n");

    idt_initialize();
    vga_writestring("[OK] IDT initialized\n");

    pic_initialize();
    vga_writestring("[OK] PIC initialized\n");

    keyboard_initialize();
    register_interrupt_handler(33, keyboard_handler);
    pic_unmask_irq(1);
    vga_writestring("[OK] Keyboard registered\n");

    enable_interrupts();
    vga_writestring("[OK] Interrupts enabled\n\n");

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_writestring("Press 'E' key to trigger a Division By Zero exception demo.\n");
    vga_writestring("The system will display the exception and continue running.\n\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    while (1) {
        halt();
    }
}
#include "../include/vga.h"
#include "../include/io.h"
#include "../include/string.h"
#include "../include/keyboard.h"
#include "../include/timer.h"
#include "../include/interrupts.h"
#include "../include/stdio.h"

// Keep serial debug from original kernel
static void serial_write(char c) {
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, c);
}

static void serial_string(const char* s) {
    while (*s) serial_write(*s++);
}

void kernel_main(void) {
    // Init serial
    outb(0x3F9, 0x00); outb(0x3FB, 0x80); outb(0x3F8, 0x01);
    outb(0x3F9, 0x00); outb(0x3FB, 0x03); outb(0x3FA, 0xC7); outb(0x3FC, 0x0B);
    
    serial_string("\n\n=== TinyOS printf Test ===\n");
    
    // Init VGA
    vga_initialize();
    vga_clear_screen(VGA_COLOR_BLUE);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    
    // Test printf - each on separate line
    printf("Test 1: String\n");
    printf("Test 2: %s\n", "Hello World");
    printf("Test 3: Char=%c\n", 'X');
    printf("Test 4: Dec=%d\n", 12345);
    printf("Test 5: Neg=%d\n", -999);
    printf("Test 6: hex=%x\n", 0xABCD);
    printf("Test 7: HEX=%X\n", 0xABCD);
    printf("Test 8: ptr=%p\n", (void*)0x12345678);
    printf("Test 9: %%\n");
    
    serial_string("[OK] printf test displayed\n");
    
    // Init interrupts
    idt_initialize();
    pic_initialize();
    
    timer_initialize(50);
    register_interrupt_handler(32, timer_handler);
    pic_unmask_irq(0);
    
    keyboard_initialize();
    register_interrupt_handler(33, keyboard_handler);
    pic_unmask_irq(1);
    
    enable_interrupts();
    
    printf("\nType something:\n> ");
    
    while (1) {
        if (keyboard_has_input()) {
            char c = keyboard_read_char();
            if (c == '\n') {
                printf("\n> ");
            } else if (c == '\b') {
                vga_putchar('\b');
            } else if (c >= 32 && c < 127) {
                vga_putchar(c);
            }
        }
    }
}

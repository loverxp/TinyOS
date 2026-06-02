#include "../include/vga.h"
#include "../include/io.h"
#include "../include/string.h"
#include "../include/keyboard.h"
#include "../include/timer.h"
#include "../include/interrupts.h"
#include "../include/stdio.h"
#include "../include/mm.h"

// Serial debug output
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
    serial_string("\n\n=== TinyOS Started ===\n");
    
    // Init VGA
    vga_initialize();
    vga_clear_screen(VGA_COLOR_BLUE);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    
    printf("=====================================\n");
    printf("      Welcome to TinyOS!             \n");
    printf("      A Simple C Operating System    \n");
    printf("=====================================\n\n");
    
    printf("System Information:\n");
    printf("  - Architecture: x86 (32-bit)\n");
    printf("  - Bootloader: Multiboot compliant\n");
    printf("  - Display: VGA Text Mode\n\n");
    
    // Test kmalloc
    mm_test();
    
    // Init interrupts
    printf("\nInitializing interrupts...\n");
    idt_initialize();
    pic_initialize();
    
    timer_initialize(50);
    register_interrupt_handler(32, timer_handler);
    pic_unmask_irq(0);
    printf("  [OK] Timer initialized (50 Hz)\n");
    
    keyboard_initialize();
    register_interrupt_handler(33, keyboard_handler);
    pic_unmask_irq(1);
    printf("  [OK] Keyboard initialized\n");
    
    enable_interrupts();
    printf("  [OK] Interrupts enabled\n");
    
    printf("\nTinyOS is ready! Type something:\n");
    printf("> ");
    
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

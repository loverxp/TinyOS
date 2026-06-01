#include "../include/vga.h"
#include "../include/io.h"
#include "../include/string.h"
#include "../include/keyboard.h"
#include "../include/timer.h"
#include "../include/interrupts.h"

// Simple debug output to serial port
static void serial_write(char c) {
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, c);
}

static void serial_string(const char* s) {
    while (*s) {
        serial_write(*s++);
    }
}

static void serial_hex(uint32_t n) {
    char hex[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        serial_write(hex[(n >> i) & 0xF]);
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
    
    serial_string("\n\n=== TinyOS Debug Log ===\n");
    
    // Initialize VGA display
    vga_initialize();
    serial_string("[DEBUG] VGA initialized\n");
    
    vga_clear_screen(VGA_COLOR_BLUE);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    
    vga_writestring("=====================================\n");
    vga_writestring("      Welcome to TinyOS!             \n");
    vga_writestring("      A Simple C Operating System    \n");
    vga_writestring("=====================================\n\n");
    
    vga_writestring("System Information:\n");
    vga_writestring("  - Architecture: x86 (32-bit)\n");
    vga_writestring("  - Bootloader: Multiboot compliant\n");
    vga_writestring("  - Display: VGA Text Mode\n\n");
    
    vga_writestring("Initializing subsystems...\n");
    serial_string("[DEBUG] Starting interrupt init...\n");
    
    // Initialize IDT FIRST (before PIC, before interrupts)
    idt_initialize();
    serial_string("[DEBUG] IDT initialized\n");
    vga_writestring("  [OK] IDT initialized\n");
    
    // Initialize PIC (mask ALL interrupts)
    pic_initialize();
    serial_string("[DEBUG] PIC initialized (all masked)\n");
    vga_writestring("  [OK] PIC initialized\n");
    
    // Register timer handler and unmask IRQ0
    timer_initialize(50);
    register_interrupt_handler(32, timer_handler);
    pic_unmask_irq(0);
    serial_string("[DEBUG] Timer ready (IRQ0 unmasked)\n");
    vga_writestring("  [OK] Timer initialized (50 Hz)\n");
    
    // Register keyboard handler and unmask IRQ1
    keyboard_initialize();
    register_interrupt_handler(33, keyboard_handler);
    pic_unmask_irq(1);
    serial_string("[DEBUG] Keyboard ready (IRQ1 unmasked)\n");
    vga_writestring("  [OK] Keyboard initialized\n");
    
    // NOW enable interrupts
    serial_string("[DEBUG] Enabling interrupts (sti)...\n");
    enable_interrupts();
    serial_string("[DEBUG] Interrupts enabled!\n");
    vga_writestring("  [OK] Interrupts enabled\n");
    
    vga_writestring("\nTinyOS is ready! Type something:\n");
    vga_writestring("> ");
    serial_string("[DEBUG] Entering main loop...\n");
    
    uint32_t tick_count = 0;
    uint32_t last_ticks = 0;
    
    while (1) {
        uint32_t current_ticks = timer_get_ticks();
        if (current_ticks != last_ticks) {
            last_ticks = current_ticks;
            tick_count++;
            if (tick_count >= 50) {
                tick_count = 0;
                serial_string("[DEBUG] 1 second passed, ticks=");
                serial_hex(current_ticks);
                serial_string("\n");
            }
        }
        
        if (keyboard_has_input()) {
            char c = keyboard_read_char();
            serial_string("[DEBUG] Key received: '");
            serial_write(c);
            serial_string("' (0x");
            serial_hex((uint8_t)c);
            serial_string(")\n");
            
            if (c == '\n') {
                vga_putchar('\n');
                vga_writestring("> ");
            } else if (c == '\b') {
                vga_putchar('\b');
            } else if (c >= 32 && c < 127) {
                vga_putchar(c);
            }
        }
    }
}

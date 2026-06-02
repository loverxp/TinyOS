#include "../include/except.h"
#include "../include/vga.h"
#include "../include/io.h"

void exception_handler(uint32_t int_no, uint32_t err_code) {
    static const char* names[] = {
        "Division By Zero", "Debug", "NMI", "Breakpoint",
        "Overflow", "Bound Range", "Invalid Opcode", "Device Not Available",
        "Double Fault", "Coprocessor Segment", "Bad TSS", "Segment Not Present",
        "Stack Fault", "General Protection Fault", "Page Fault", "Reserved",
        "x87 FPU Error", "Alignment Check", "Machine Check", "SIMD FPU Error",
        "Virtualization", "Control Protection", "Reserved", "Reserved",
        "Reserved", "Reserved", "Reserved", "Reserved",
        "Hypervisor Injection", "VMM Communication", "Security", "Reserved"
    };

    vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
    vga_writestring("\n*** EXCEPTION: ");
    if (int_no < 32) {
        vga_writestring(names[int_no]);
    } else {
        vga_write_dec(int_no);
    }
    vga_writestring(" ***\n");
    vga_writestring("    Error Code: 0x");
    vga_write_hex(err_code);
    vga_writestring("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_writestring("    System continues...\n");
}

void exceptions_init(void) {
}

void exception_test(void) {
    volatile int a = 10;
    volatile int b = 0;
    volatile int c = a / b;
    (void)c;
}
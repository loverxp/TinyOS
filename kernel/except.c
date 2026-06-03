#include "../include/except.h"
#include "../include/vga.h"
#include "../include/io.h"
#include "../include/stdio.h"

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
    printf("\n*** EXCEPTION: %s ***\n", int_no < 32 ? names[int_no] : "Unknown");
    printf("    Error Code: 0x%x\n", err_code);
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    printf("    System continues...\n");
}

void exceptions_init(void) {
}

void exception_test(void) {
    volatile int a = 10;
    volatile int b = 0;
    volatile int c = a / b;
    (void)c;
}
#include "../include/vga.h"
#include "../include/io.h"
#include "../include/interrupts.h"
#include "../include/keyboard.h"
#include "../include/timer.h"
#include "../include/shell.h"
#include "../include/pmm.h"
#include "../include/mm.h"
#include "../include/gdt.h"
#include "../include/tss.h"
#include "../include/paging.h"
#include "../include/stdio.h"

// User mode entry points (from user.asm)
extern void run_user_task(void (*entry)(void));
extern void user_main(void);

// Test user mode switching from kernel
void test_user_mode(void) {
    printf("Switching to Ring 3 (user mode)...\n");
    run_user_task(user_main);
    printf("Back in kernel mode! Test passed.\n");
}

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

// Called every second from timer interrupt - update VGA status bar
void on_timer_second(void) {
    uint32_t ticks = timer_get_ticks();
    uint32_t secs = ticks / 50;

    // Get memory stats
    uint32_t heap_used = kmalloc_get_used();
    uint32_t heap_total = kmalloc_get_total();
    uint32_t mem_free = pmm_get_free_pages() * 4;  // in KB

    size_t save_row = vga_get_cursor_row();
    size_t save_col = vga_get_cursor_column();
    vga_set_cursor(VGA_HEIGHT - 1, 0);
    printf("Uptime: %us  Heap: %uK/%uK  Mem: %uM free        ", secs, heap_used / 1024, heap_total / 1024, mem_free / 1024);
    vga_set_cursor(save_row, save_col);
}

void kernel_main(uint32_t multiboot_info_addr) {
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
    printf("TinyOS v0.1 - Kernel Loaded\n");
    printf("==========================\n\n");
    serial_string("[OK] VGA\n");

    // Initialize physical memory manager
    pmm_init(multiboot_info_addr);
    printf("[OK] Physical memory: %u MB (%u free pages)\n", pmm_get_total_memory_kb() / 1024, pmm_get_free_pages());
    serial_string("[OK] PMM\n");

    // Initialize kernel heap allocator
    mm_init();
    printf("[OK] Kernel heap initialized\n");
    serial_string("[OK] MM\n");

    // Initialize paging (identity map first 8MB)
    paging_init();
    serial_string("[OK] Paging\n");

    // Initialize TSS for Ring 3 -> Ring 0 transitions
    // Allocate a dedicated 4KB kernel stack for TSS
    static uint8_t tss_kernel_stack[4096] __attribute__((aligned(16)));
    tss_init((uint32_t)tss_kernel_stack + 4096);
    printf("[OK] TSS initialized\n");
    serial_string("[OK] TSS\n");

    idt_initialize();
    printf("[OK] IDT initialized\n");
    serial_string("[OK] IDT\n");

    pic_initialize();
    printf("[OK] PIC initialized\n");
    serial_string("[OK] PIC\n");

    timer_initialize(50);
    timer_register_second_callback(on_timer_second);
    register_interrupt_handler(32, timer_handler);
    pic_unmask_irq(0);
    printf("[OK] Timer initialized (50 Hz)\n");
    serial_string("[OK] Timer\n");

    keyboard_initialize();
    register_interrupt_handler(33, keyboard_handler);
    pic_unmask_irq(1);
    printf("[OK] Keyboard initialized\n");
    serial_string("[OK] Keyboard\n");

    shell_init();
    printf("[OK] Shell initialized\n");
    serial_string("[OK] Shell\n");

    enable_interrupts();
    printf("[OK] Interrupts enabled\n\n");
    serial_string("[OK] Interrupts enabled\n");

    printf("Type 'help' for available commands.\n\n");

    serial_string("Ready, entering main loop...\n");

    // Event-driven main loop: shell runs in kernel mode (Ring 0)
    while (1) {
        halt();
    }
}
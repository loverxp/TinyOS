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
#include "../include/scheduler.h"
#include "../include/signal.h"
#include "../include/ata.h"
#include "../include/fat16.h"
#include "../include/pci.h"
#include "../include/ne2000.h"
#include "../include/net.h"
#include "../include/serial.h"
#include "../include/vbe.h"
#include "../include/ipc.h"
#include "../include/mbr.h"
#include "../include/vfs.h"

// User mode entry points (from user.asm)
extern void run_user_task(void (*entry)(void));
extern void user_main(void);

// Dedicated kernel stack for TSS Ring 3 → Ring 0 transitions
// Used by the idle task and as fallback when per-task stacks are not set up.
uint8_t tss_kernel_stack[4096] __attribute__((aligned(16)));

// Test user mode switching from kernel
void test_user_mode(void) {
    printf("Switching to Ring 3 (user mode)...\n");
    run_user_task(user_main);
    printf("Back in kernel mode! Test passed.\n");
}

// VGA video memory
static volatile uint16_t* const vga_mem = (uint16_t*)0xB8000;

// Called every second from timer interrupt - update VGA status bar only (no serial, no cursor move)
void on_timer_second(void) {
    uint32_t ticks = timer_get_ticks();
    uint32_t secs = ticks / 50;

    uint32_t heap_used = kmalloc_get_used();
    uint32_t heap_total = kmalloc_get_total();
    uint32_t mem_free = pmm_get_free_pages() * 4;  // in KB

    char buf[80];
    int n = sprintf(buf, "Uptime: %us  Heap: %uK/%uK  Mem: %uM free", secs, heap_used / 1024, heap_total / 1024, mem_free / 1024);

    uint8_t color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLUE);
    int row = VGA_HEIGHT - 1;
    int col;
    for (col = 0; col < n && col < VGA_WIDTH; col++) {
        vga_mem[row * VGA_WIDTH + col] = (uint16_t)buf[col] | (uint16_t)color << 8;
    }
    // Clear rest of the status line
    for (; col < VGA_WIDTH; col++) {
        vga_mem[row * VGA_WIDTH + col] = (uint16_t)' ' | (uint16_t)color << 8;
    }
}

void kernel_main(uint32_t multiboot_info_addr) {
    // Initialize serial port (COM1)
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x80);
    outb(0x3F8, 0x01);
    outb(0x3F9, 0x00);
    outb(0x3FB, 0x03);
    outb(0x3FA, 0x07);   // FCR: enable FIFO, clear, trigger at 1 byte
    outb(0x3FC, 0x0B);

    serial_writestring("=== TinyOS Debug ===\n");

    gdt_init();
    serial_writestring("[OK] GDT\n");

    vga_initialize();
    vga_save_font();
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    printf("TinyOS v0.1 - Kernel Loaded\n");
    printf("==========================\n\n");
    serial_writestring("[OK] VGA\n");

    // Initialize physical memory manager
    pmm_init(multiboot_info_addr);
    printf("[OK] Physical memory: %u MB (%u free pages)\n", pmm_get_total_memory_kb() / 1024, pmm_get_free_pages());
    serial_writestring("[OK] PMM\n");

    // Initialize kernel heap allocator
    mm_init();
    printf("[OK] Kernel heap initialized\n");
    serial_writestring("[OK] MM\n");

    // Initialize paging (identity map first 8MB)
    paging_init();
    serial_writestring("[OK] Paging\n");

    // Initialize TSS for Ring 3 -> Ring 0 transitions
    // Uses the global tss_kernel_stack declared above
    tss_init((uint32_t)tss_kernel_stack + 4096);
    printf("[OK] TSS initialized\n");
    serial_writestring("[OK] TSS\n");

    idt_initialize();
    printf("[OK] IDT initialized\n");
    serial_writestring("[OK] IDT\n");

    pic_initialize();
    printf("[OK] PIC initialized\n");
    serial_writestring("[OK] PIC\n");

    timer_initialize(50);
    timer_register_second_callback(on_timer_second);
    register_interrupt_handler(32, timer_handler);
    pic_unmask_irq(0);
    printf("[OK] Timer initialized (50 Hz)\n");
    serial_writestring("[OK] Timer\n");

    keyboard_initialize();
    register_interrupt_handler(33, keyboard_handler);
    pic_unmask_irq(1);
    printf("[OK] Keyboard initialized\n");
    serial_writestring("[OK] Keyboard\n");

    /* Initialize serial RX interrupt (COM1, IRQ 4) */
    serial_init();
    register_interrupt_handler(36, serial_handler);
    pic_unmask_irq(4);
    printf("[OK] Serial shell (COM1, IRQ 4)\n");
    serial_writestring("[OK] Serial RX\n");

    shell_init();
    printf("[OK] Shell initialized\n");
    serial_writestring("[OK] Shell\n");

    enable_interrupts();
    printf("[OK] Interrupts enabled\n\n");
    serial_writestring("[OK] Interrupts enabled\n");

    scheduler_init();
    serial_writestring("[OK] Scheduler\n");

    signal_init();

    ipc_init();
    serial_writestring("[OK] IPC\n");

    /* Initialize ATA disk driver */
    if (ata_init() == 0) {
        printf("[OK] ATA disk detected\n");
        serial_writestring("[OK] ATA\n");

        /* Parse MBR partition table */
        mbr_info_t* mbr = mbr_get_info();
        if (mbr_init(mbr) == 0) {
            printf("[OK] MBR: %d partition(s), FAT16 at index %d\n",
                   mbr->count, mbr->fat16_partition);
        }

        /* Initialize VFS and register DevFS backend */
        vfs_init();
        extern const vfs_backend_ops_t devfs_ops;
        vfs_register_backend(&devfs_ops);
        printf("[OK] VFS initialized (DevFS: /dev/null, /dev/zero)\n");
        serial_writestring("[OK] VFS\n");

        /* Initialize FAT16 filesystem */
        if (fat16_init() == 0) {
            printf("[OK] FAT16 filesystem mounted\n");
            serial_writestring("[OK] FAT16\n");
        } else {
            printf("[!!] FAT16 init failed\n");
        }
    } else {
        printf("[!!] No ATA disk (use -drive flag)\n");
    }

    /* Scan PCI bus */
    pci_scan();
    printf("[OK] PCI: %d device(s)\n", pci_get_device_count());
    serial_writestring("[OK] PCI\n");

    /* Initialize NE2000 network driver */
    if (ne2000_init() == 0) {
        printf("[OK] NE2000 network card\n");
        serial_writestring("[OK] NE2000\n");

        /* Register NE2000 IRQ handler */
        register_interrupt_handler(32 + 11, ne2000_handler);  /* IRQ 11 */
        pic_unmask_irq(11);

        /* Initialize network stack */
        /* QEMU user-mode: host=10.0.2.2, guest=10.0.2.15, gateway=10.0.2.2 */
        net_init(IP4(10, 0, 2, 15), IP4(10, 0, 2, 2), IP4(255, 255, 255, 0));
        printf("[OK] Network stack (10.0.2.15)\n");
        serial_writestring("[OK] NET\n");

        /* Set NE2000 receive callback to net handler */
        ne2000_set_recv_callback(net_recv_handler);
    } else {
        printf("[!!] No NE2000 NIC (use -netdev + -device flags)\n");
    }

    /* Check VBE graphics capability (GUI is NOT started automatically).
       Type 'gui' at the shell to enter graphics mode. */
    if (vbe_detect()) {
        printf("[OK] VBE graphics capable (800x600x32). Type 'gui' to start GUI.\n");
    } else {
        printf("[!!] VBE not available — GUI not supported.\n");
    }

    printf("Type 'help' for available commands.\n\n");

    serial_writestring("Ready, entering main loop...\n");

    // Idle main loop — shell runs via interrupt-driven callbacks
    while (1) {
        halt();
    }
}
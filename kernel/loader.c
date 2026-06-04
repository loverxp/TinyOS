// loader.c - User program loader
// Loads an embedded user binary to memory and executes it in Ring 3

#include "../include/loader.h"
#include "../include/vga.h"
#include "../include/string.h"
#include "../include/pmm.h"
#include "../include/stdio.h"
#include "../include/io.h"

// External assembly functions (from user.asm)
extern void run_user_task_ex(void (*entry)(void), void* user_esp);

// Embedded user binary (from embedded_user.asm)
extern uint8_t embedded_user_start[];
extern uint8_t embedded_user_end[];

// External assembly functions
extern void vga_initialize(void);
extern void vga_set_color(enum vga_color fg, enum vga_color bg);
extern void vga_clear_screen(enum vga_color bg);
extern void vga_set_cursor(size_t row, size_t column);
extern void keyboard_register_char_callback(void (*callback)(char));
extern void shell_init(void);
extern void vga_set_mode03h(void);

// Target execution address for user programs
#define USER_PROG_BASE  0x400000

// #region debug-point E:loader - serial helpers
static void dbg_serial_write(char c) {
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, c);
}

static void dbg_serial_string(const char* s) {
    while (*s) dbg_serial_write(*s++);
}

static void dbg_serial_hex(uint32_t n) {
    char hex[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        dbg_serial_write(hex[(n >> i) & 0xF]);
    }
}
// #endregion

void run_loaded_user(void) {
    uint32_t size = embedded_user_end - embedded_user_start;
    
    printf("Loading user program (%u bytes) to 0x%x...\n", size, USER_PROG_BASE);
    
    // #region debug-point E:loader - Verify binary is valid
    dbg_serial_string("[LOADER] binary size=");
    dbg_serial_hex(size);
    dbg_serial_string(" first_bytes=");
    for (int i = 0; i < 8 && i < (int)size; i++) {
        dbg_serial_hex(embedded_user_start[i]);
        dbg_serial_string(" ");
    }
    dbg_serial_string("\r\n");
    // #endregion
    
    // Copy program binary to target address
    memcpy((void*)USER_PROG_BASE, embedded_user_start, size);
    
    // Allocate a page for user stack (4KB)
    void* user_stack = pmm_alloc_page();
    if (!user_stack) {
        printf("ERROR: Failed to allocate user stack!\n");
        return;
    }
    
    uint32_t user_esp = (uint32_t)user_stack + 4096;  // stack grows down
    
    printf("User stack at 0x%x, switching to Ring 3...\n\n", (uint32_t)user_stack);
    
    // Send EOI for IRQ1 before switching to Ring 3.
    // This function may be called from the keyboard ISR (via shell command handler),
    // and the EOI in irq_handler() will never be reached because we never return.
    // Without this EOI, the PIC masks keyboard interrupts permanently.
    outb(0x20, 0x20);  // Send EOI to master PIC for IRQ1
    
    dbg_serial_string("[LOADER] switching to Ring 3 at 0x400000...\r\n");
    
    // Run the user program with custom stack
    run_user_task_ex((void (*)(void))USER_PROG_BASE, (void*)user_esp);
    
    dbg_serial_string("[LOADER] returned from user program\r\n");
    
    // Restore VGA text mode before re-initializing display
    vga_set_mode03h();
    
    // Free the user stack page
    pmm_free_page(user_stack);
    
    // Restore text mode and VGA state
    vga_initialize();
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear_screen(VGA_COLOR_BLACK);
    
    printf("\nUser program finished, back in kernel mode.\n");
    
    // Reinitialize shell
    shell_init();
}
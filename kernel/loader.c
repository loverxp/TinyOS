// loader.c - User program loader
// Loads an embedded user binary to memory and executes it in Ring 3

#include "../include/loader.h"
#include "../include/vga.h"
#include "../include/string.h"
#include "../include/pmm.h"
#include "../include/stdio.h"
#include "../include/io.h"
#include "../include/elf.h"

// External assembly functions (from user.asm)
extern void run_user_task_ex(void (*entry)(void), void* user_esp);

// Embedded user binary (from embedded_user.asm)
extern uint8_t embedded_user_start[];
extern uint8_t embedded_user_end[];

// Embedded hello binary (from embedded_hello.asm)
extern uint8_t embedded_hello_start[];
extern uint8_t embedded_hello_end[];

// Embedded echo binary (from embedded_echo.asm)
extern uint8_t embedded_echo_start[];
extern uint8_t embedded_echo_end[];

// Embedded clear binary (from embedded_clear.asm)
extern uint8_t embedded_clear_start[];
extern uint8_t embedded_clear_end[];

// Embedded help binary (from embedded_help.asm)
extern uint8_t embedded_help_start[];
extern uint8_t embedded_help_end[];

// Embedded forktest binary (from embedded_forktest.asm)
extern uint8_t embedded_forktest_start[];
extern uint8_t embedded_forktest_end[];

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

// ELF loader: validate, load segments, return entry point.
// Returns 0 on success, -1 on error.
static int elf_load(const uint8_t* elf_data, uint32_t size) {
    const Elf32_Ehdr* ehdr = (const Elf32_Ehdr*)elf_data;
    
    if (!elf_validate(ehdr)) {
        printf("ERROR: Invalid ELF header\n");
        return -1;
    }
    
    dbg_serial_string("[ELF] entry=0x");
    dbg_serial_hex(ehdr->e_entry);
    dbg_serial_string(" phnum=");
    dbg_serial_hex(ehdr->e_phnum);
    dbg_serial_string("\r\n");
    
    // Validate program header table is within bounds
    if (ehdr->e_phoff + ehdr->e_phnum * ehdr->e_phentsize > size) {
        printf("ERROR: Program headers out of bounds\n");
        return -1;
    }
    
    // Iterate program headers and load PT_LOAD segments
    for (int i = 0; i < ehdr->e_phnum; i++) {
        const Elf32_Phdr* phdr = (const Elf32_Phdr*)(elf_data + ehdr->e_phoff + i * ehdr->e_phentsize);
        
        if (phdr->p_type != PT_LOAD) continue;
        
        dbg_serial_string("[ELF] LOAD segment: vaddr=0x");
        dbg_serial_hex(phdr->p_vaddr);
        dbg_serial_string(" filesz=");
        dbg_serial_hex(phdr->p_filesz);
        dbg_serial_string(" memsz=");
        dbg_serial_hex(phdr->p_memsz);
        dbg_serial_string(" offset=0x");
        dbg_serial_hex(phdr->p_offset);
        dbg_serial_string("\r\n");
        
        // Verify segment data is within bounds
        if (phdr->p_offset + phdr->p_filesz > size) {
            printf("ERROR: Segment data out of bounds\n");
            return -1;
        }
        
        // Copy segment data to target virtual address
        memcpy((void*)phdr->p_vaddr, elf_data + phdr->p_offset, phdr->p_filesz);
        
        // Zero-fill BSS region (memsz > filesz)
        if (phdr->p_memsz > phdr->p_filesz) {
            memset((void*)(phdr->p_vaddr + phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);
        }
    }
    
    return 0;
}

void run_loaded_user(void) {
    uint32_t size = embedded_user_end - embedded_user_start;
    
    if (size < sizeof(Elf32_Ehdr)) {
        printf("ERROR: gfxsnake.elf is too small!\n");
        return;
    }
    
    printf("Loading gfxsnake.elf (%u bytes)...\n", size);
    
    const Elf32_Ehdr* ehdr = (const Elf32_Ehdr*)embedded_user_start;
    uint32_t entry = ehdr->e_entry;
    
    // Load ELF segments
    if (elf_load(embedded_user_start, size) != 0) {
        printf("ERROR: Failed to load gfxsnake.elf\n");
        return;
    }
    
    // Allocate a page for user stack (4KB)
    void* user_stack = pmm_alloc_page();
    if (!user_stack) {
        printf("ERROR: Failed to allocate user stack!\n");
        return;
    }
    
    uint32_t user_esp = (uint32_t)user_stack + 4096;  // stack grows down
    
    printf("Entry: 0x%x, Stack at 0x%x, switching to Ring 3...\n\n", entry, (uint32_t)user_stack);
    
    // Send EOI for IRQ1 before switching to Ring 3.
    // This function may be called from the keyboard ISR (via shell command handler),
    // and the EOI in irq_handler() will never be reached because we never return.
    // Without this EOI, the PIC masks keyboard interrupts permanently.
    outb(0x20, 0x20);  // Send EOI to master PIC for IRQ1
    
    dbg_serial_string("[LOADER] switching to Ring 3 at entry 0x");
    dbg_serial_hex(entry);
    dbg_serial_string("...\r\n");
    
    // Run the user program with custom stack
    run_user_task_ex((void (*)(void))entry, (void*)user_esp);
    
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

void run_hello_user(void) {
    uint32_t size = embedded_hello_end - embedded_hello_start;
    
    if (size < sizeof(Elf32_Ehdr)) {
        printf("ERROR: hello.elf is too small!\n");
        return;
    }
    
    printf("Loading hello.elf (%u bytes)...\n", size);
    
    const Elf32_Ehdr* ehdr = (const Elf32_Ehdr*)embedded_hello_start;
    uint32_t entry = ehdr->e_entry;
    
    // Load ELF segments
    if (elf_load(embedded_hello_start, size) != 0) {
        printf("ERROR: Failed to load hello.elf\n");
        return;
    }
    
    // Allocate a page for user stack (4KB)
    void* user_stack = pmm_alloc_page();
    if (!user_stack) {
        printf("ERROR: Failed to allocate user stack!\n");
        return;
    }
    
    uint32_t user_esp = (uint32_t)user_stack + 4096;
    
    printf("Entry: 0x%x, Stack: 0x%x, switching to Ring 3...\n\n", entry, (uint32_t)user_stack);
    
    // Send EOI for IRQ1
    outb(0x20, 0x20);
    
    // Run the user program with custom stack
    run_user_task_ex((void (*)(void))entry, (void*)user_esp);
    
    // Restore VGA text mode (preserves existing VGA text buffer content)
    vga_set_mode03h();
    
    // Free the user stack page
    pmm_free_page(user_stack);
    
    // Initialize VGA driver state without clearing the screen
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    // Move cursor to first free line after the user program's output
    size_t crow = vga_get_cursor_row();
    if (crow < VGA_HEIGHT - 2) crow++;
    vga_set_cursor(crow, 0);

    printf("\nHello program finished, back in kernel mode.\n");
    
    // Reinitialize shell
    shell_init();
}

/* Generic helper to load and run an embedded ELF user program.
 * Returns after the user program exits via syscall 0.
 * On success, restores shell. On error, prints message.
 */
static void run_embedded_elf(const char* name, const uint8_t* start, const uint8_t* end) {
    uint32_t size = end - start;

    if (size < sizeof(Elf32_Ehdr)) {
        printf("ERROR: %s is too small!\n", name);
        return;
    }

    const Elf32_Ehdr* ehdr = (const Elf32_Ehdr*)start;
    uint32_t entry = ehdr->e_entry;

    if (elf_load(start, size) != 0) {
        printf("ERROR: Failed to load %s\n", name);
        return;
    }

    void* user_stack = pmm_alloc_page();
    if (!user_stack) {
        printf("ERROR: Failed to allocate user stack for %s\n", name);
        return;
    }

    uint32_t user_esp = (uint32_t)user_stack + 4096;

    // Send EOI for IRQ1 before Ring 3 switch
    outb(0x20, 0x20);

    run_user_task_ex((void (*)(void))entry, (void*)user_esp);

    // Restore VGA text mode
    vga_set_mode03h();
    pmm_free_page(user_stack);

    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    size_t crow = vga_get_cursor_row();
    if (crow < VGA_HEIGHT - 2) crow++;
    vga_set_cursor(crow, 0);

    shell_init();
}

/* External user command arguments buffer */
extern char user_cmd_args[256];

void run_echo_user(const char* text) {
    /* Copy echo text to user command args buffer */
    strcpy(user_cmd_args, text);
    run_embedded_elf("echo.elf", embedded_echo_start, embedded_echo_end);
}

void run_clear_user(void) {
    run_embedded_elf("clear.elf", embedded_clear_start, embedded_clear_end);
}

void run_help_user(void) {
    run_embedded_elf("help.elf", embedded_help_start, embedded_help_end);
}

void run_forktest_user(void) {
    run_embedded_elf("forktest.elf", embedded_forktest_start, embedded_forktest_end);
}
// loader.c - User program loader
// Loads an embedded user binary to memory and executes it in Ring 3

#include "../include/loader.h"
#include "../include/vga.h"
#include "../include/string.h"
#include "../include/pmm.h"
#include "../include/stdio.h"

// External assembly functions (from user.asm)
extern void run_user_task_ex(void (*entry)(void), void* user_esp);

// Embedded user binary (from embedded_user.asm)
extern uint8_t embedded_user_start[];
extern uint8_t embedded_user_end[];

// Target execution address for user programs
#define USER_PROG_BASE  0x400000

void run_loaded_user(void) {
    uint32_t size = embedded_user_end - embedded_user_start;
    
    printf("Loading user program (%u bytes) to 0x%x...\n", size, USER_PROG_BASE);
    
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
    
    // Run the user program with custom stack
    run_user_task_ex((void (*)(void))USER_PROG_BASE, (void*)user_esp);
    
    // Free the user stack page
    pmm_free_page(user_stack);
    
    printf("\nUser program finished, back in kernel mode.\n");
}
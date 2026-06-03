#include "../include/shell.h"
#include "../include/vga.h"
#include "../include/keyboard.h"
#include "../include/timer.h"
#include "../include/string.h"
#include "../include/pmm.h"
#include "../include/mm.h"
#include "../include/stdio.h"
#include "../include/loader.h"

#define LINE_BUF_SIZE 256

static char line_buffer[LINE_BUF_SIZE];
static size_t line_pos = 0;

static void shell_prompt(void) {
    vga_writestring("TinyOS> ");
}

static uint32_t parse_hex(const char* s) {
    uint32_t val = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    while (*s) {
        char c = *s++;
        val <<= 4;
        if (c >= '0' && c <= '9') val |= c - '0';
        else if (c >= 'a' && c <= 'f') val |= c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') val |= c - 'A' + 10;
        else break;
    }
    return val;
}

static void shell_handle_command(const char* cmd) {
    // Skip leading spaces
    while (*cmd == ' ') cmd++;

    if (*cmd == '\0') return;

    if (strcmp(cmd, "help") == 0) {
            printf("Commands:\n");
            printf("  help       - Show this help\n");
            printf("  clear      - Clear screen\n");
            printf("  uptime     - Show system uptime\n");
            printf("  meminfo    - Show memory usage\n");
            printf("  alloc [N]  - Allocate N pages (default: 1)\n");
            printf("  free 0xADDR- Free a page by address\n");
            printf("  except     - Trigger Division By Zero\n");
            printf("  kmtest     - Run kmalloc/kfree test\n");
            printf("  echo <txt> - Echo text\n");
            printf("  testuser   - Switch to Ring 3 and return\n");
            printf("  runuser    - Load and run external user program\n");
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear_screen(VGA_COLOR_BLACK);
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        printf("TinyOS v0.1\n\n");
    } else if (strcmp(cmd, "uptime") == 0) {
        uint32_t ticks = timer_get_ticks();
        uint32_t secs = ticks / 50;
        uint32_t ms = (ticks % 50) * 20;
        printf("Uptime: %u.%u seconds\n", secs, ms);
    } else if (strcmp(cmd, "meminfo") == 0) {
        uint32_t total_kb = pmm_get_total_memory_kb();
        uint32_t free_pg = pmm_get_free_pages();
        uint32_t used_pg = pmm_get_used_pages();
        uint32_t total_pg = pmm_get_total_pages();
        printf("Memory:\n");
        printf("  Total: %u MB (%u pages)\n", total_kb / 1024, total_pg);
        printf("  Used:  %u pages (%u KB)\n", used_pg, used_pg * 4);
        printf("  Free:  %u pages (%u KB)\n", free_pg, free_pg * 4);
    } else if (strncmp(cmd, "alloc", 5) == 0) {
        uint32_t count = 1;
        const char* arg = cmd + 5;
        while (*arg == ' ') arg++;
        if (*arg >= '0' && *arg <= '9') {
            count = 0;
            while (*arg >= '0' && *arg <= '9') {
                count = count * 10 + (*arg++ - '0');
            }
        }
        if (count > 64) count = 64;
        for (uint32_t i = 0; i < count; i++) {
            void* page = pmm_alloc_page();
            if (page) {
                printf("  Allocated: 0x%x\n", (uint32_t)page);
            } else {
                printf("  Out of memory!\n");
                break;
            }
        }
    } else if (strncmp(cmd, "free ", 5) == 0) {
        const char* arg = cmd + 5;
        while (*arg == ' ') arg++;
        uint32_t addr = parse_hex(arg);
        if (addr && (addr % 4096) == 0) {
            pmm_free_page((void*)addr);
            printf("  Freed page: 0x%x\n", addr);
        } else {
            printf("  Invalid address (must be 4KB-aligned)\n");
        }
    } else if (strcmp(cmd, "except") == 0) {
        printf("Triggering Division By Zero...\n");
        asm volatile("int $0");
    } else if (strcmp(cmd, "kmtest") == 0) {
        mm_test();
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        const char* text = cmd + 5;
        while (*text == ' ') text++;
        printf("%s\n", text);
    } else if (strcmp(cmd, "runuser") == 0) {
        run_loaded_user();
    } else if (strcmp(cmd, "testuser") == 0) {
        extern void test_user_mode(void);
        test_user_mode();
    } else {
        printf("Unknown command: %s\n", cmd);
        printf("Type 'help' for available commands.\n");
    }
}

void shell_char_callback(char c) {
    if (c == '\n') {
        vga_putchar('\n');
        line_buffer[line_pos] = '\0';
        shell_handle_command(line_buffer);
        line_pos = 0;
        shell_prompt();
    } else if (c == '\b') {
        if (line_pos > 0) {
            line_pos--;
            vga_putchar('\b');
        }
    } else if (c >= 32 && c < 127) {
        if (line_pos < LINE_BUF_SIZE - 1) {
            line_buffer[line_pos++] = c;
            vga_putchar(c);
        }
    }
}

void shell_init(void) {
    keyboard_register_char_callback(shell_char_callback);
    shell_prompt();
}
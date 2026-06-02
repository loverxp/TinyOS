#include "../include/shell.h"
#include "../include/vga.h"
#include "../include/keyboard.h"
#include "../include/timer.h"
#include "../include/string.h"

#define LINE_BUF_SIZE 256

static char line_buffer[LINE_BUF_SIZE];
static size_t line_pos = 0;

static void shell_prompt(void) {
    vga_writestring("TinyOS> ");
}

static void shell_handle_command(const char* cmd) {
    // Skip leading spaces
    while (*cmd == ' ') cmd++;

    if (*cmd == '\0') return;

    if (strcmp(cmd, "help") == 0) {
        vga_writestring("Commands:\n");
        vga_writestring("  help    - Show this help\n");
        vga_writestring("  clear   - Clear screen\n");
        vga_writestring("  uptime  - Show system uptime\n");
        vga_writestring("  except  - Trigger Division By Zero exception\n");
        vga_writestring("  echo    - Echo text\n");
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear_screen(VGA_COLOR_BLACK);
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        vga_writestring("TinyOS v0.1\n\n");
    } else if (strcmp(cmd, "uptime") == 0) {
        uint32_t ticks = timer_get_ticks();
        uint32_t secs = ticks / 50;
        uint32_t ms = (ticks % 50) * 20;
        vga_writestring("Uptime: ");
        vga_write_dec(secs);
        vga_writestring(".");
        vga_write_dec(ms);
        vga_writestring(" seconds\n");
    } else if (strcmp(cmd, "except") == 0) {
        vga_writestring("Triggering Division By Zero...\n");
        asm volatile("int $0");
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        const char* text = cmd + 5;
        while (*text == ' ') text++;
        vga_writestring(text);
        vga_putchar('\n');
    } else {
        vga_writestring("Unknown command: ");
        vga_writestring(cmd);
        vga_putchar('\n');
        vga_writestring("Type 'help' for available commands.\n");
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
#ifndef VGA_H
#define VGA_H

#include "types.h"

// VGA text mode dimensions
#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_WORK_HEIGHT 24  // Last row reserved for status bar

// VGA color codes
enum vga_color {
    VGA_COLOR_BLACK         = 0,
    VGA_COLOR_BLUE          = 1,
    VGA_COLOR_GREEN         = 2,
    VGA_COLOR_CYAN          = 3,
    VGA_COLOR_RED           = 4,
    VGA_COLOR_MAGENTA       = 5,
    VGA_COLOR_BROWN         = 6,
    VGA_COLOR_LIGHT_GREY    = 7,
    VGA_COLOR_DARK_GREY     = 8,
    VGA_COLOR_LIGHT_BLUE    = 9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,
    VGA_COLOR_WHITE         = 15,
};

// Create a color byte from foreground and background colors
uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg);

// Initialize VGA display
void vga_initialize(void);

// Set text color
void vga_set_color(enum vga_color fg, enum vga_color bg);

// Clear screen with specified background color
void vga_clear_screen(enum vga_color bg);

// Put a character at current position
void vga_putchar(char c);

// Write a string to VGA
void vga_writestring(const char* data);

// Write a number in decimal
void vga_write_dec(uint32_t num);

// Write a number in hexadecimal
void vga_write_hex(uint32_t num);

// Set cursor position (row, column)
void vga_set_cursor(size_t row, size_t column);

// Get current cursor row
size_t vga_get_cursor_row(void);

// Get current cursor column
size_t vga_get_cursor_column(void);

// Save VGA font data from plane 2 (must call at boot before any mode switching)
void vga_save_font(void);

#endif // VGA_H

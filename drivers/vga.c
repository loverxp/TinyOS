#include "../include/vga.h"
#include "../include/string.h"

// VGA text mode buffer location
static uint16_t* const VGA_MEMORY = (uint16_t*)0xB8000;

// Current cursor position and color
static size_t vga_row;
static size_t vga_column;
static uint8_t vga_color;
static uint16_t* vga_buffer;

// Create a VGA entry (character + color)
static inline uint16_t vga_entry(unsigned char c, uint8_t color) {
    return (uint16_t)c | (uint16_t)color << 8;
}

// Create a color byte (foreground + background)
uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

// Initialize VGA display
void vga_initialize(void) {
    vga_row = 0;
    vga_column = 0;
    vga_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_buffer = VGA_MEMORY;
    
    // Clear screen with default color
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = vga_entry(' ', vga_color);
        }
    }
}

// Set text color
void vga_set_color(enum vga_color fg, enum vga_color bg) {
    vga_color = vga_entry_color(fg, bg);
}

// Clear screen with specified background color
void vga_clear_screen(enum vga_color bg) {
    vga_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, bg);
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            vga_buffer[index] = vga_entry(' ', vga_color);
        }
    }
    vga_row = 0;
    vga_column = 0;
}

// Scroll screen up by one line (within work area, preserve status line)
static void vga_scroll(void) {
    // Move all lines up (rows 1..23 → rows 0..22)
    for (size_t y = 0; y < VGA_WORK_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t src_index = (y + 1) * VGA_WIDTH + x;
            const size_t dst_index = y * VGA_WIDTH + x;
            vga_buffer[dst_index] = vga_buffer[src_index];
        }
    }
    
    // Clear last work line (row 23)
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        const size_t index = (VGA_WORK_HEIGHT - 1) * VGA_WIDTH + x;
        vga_buffer[index] = vga_entry(' ', vga_color);
    }
    
    vga_row = VGA_WORK_HEIGHT - 1;
}

// Update hardware cursor position
static void vga_update_cursor(void) {
    uint16_t pos = vga_row * VGA_WIDTH + vga_column;
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// Put a character at current position
void vga_putchar(char c) {
    if (c == '\n') {
        vga_column = 0;
        vga_row++;
        if (vga_row >= VGA_WORK_HEIGHT) {
            vga_scroll();
        }
    } else if (c == '\t') {
        vga_column += 4;
        if (vga_column >= VGA_WIDTH) {
            vga_column = 0;
            vga_row++;
            if (vga_row >= VGA_WORK_HEIGHT) {
                vga_scroll();
            }
        }
    } else if (c == '\b') {
        if (vga_column > 0) {
            vga_column--;
            const size_t index = vga_row * VGA_WIDTH + vga_column;
            vga_buffer[index] = vga_entry(' ', vga_color);
        }
    } else if (c >= 32 && c < 127) {
        const size_t index = vga_row * VGA_WIDTH + vga_column;
        vga_buffer[index] = vga_entry((unsigned char)c, vga_color);
        vga_column++;
        if (vga_column >= VGA_WIDTH) {
            vga_column = 0;
            vga_row++;
            if (vga_row >= VGA_WORK_HEIGHT) {
                vga_scroll();
            }
        }
    }
    
    vga_update_cursor();
}

// Write a string to VGA
void vga_writestring(const char* data) {
    size_t len = strlen(data);
    for (size_t i = 0; i < len; i++) {
        vga_putchar(data[i]);
    }
}

// Write a number in decimal
void vga_write_dec(uint32_t num) {
    if (num == 0) {
        vga_putchar('0');
        return;
    }
    
    char buf[12];
    int i = 0;
    while (num > 0) {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    while (i > 0) {
        vga_putchar(buf[--i]);
    }
}

// Write a number in hexadecimal
void vga_write_hex(uint32_t num) {
    vga_writestring("0x");
    
    if (num == 0) {
        vga_putchar('0');
        return;
    }
    
    char buf[9];
    int i = 0;
    while (num > 0) {
        int digit = num % 16;
        if (digit < 10) {
            buf[i++] = '0' + digit;
        } else {
            buf[i++] = 'A' + (digit - 10);
        }
        num /= 16;
    }
    
    while (i > 0) {
        vga_putchar(buf[--i]);
    }
}

// Set cursor position
void vga_set_cursor(size_t row, size_t column) {
    if (row < VGA_HEIGHT && column < VGA_WIDTH) {
        vga_row = row;
        vga_column = column;
        vga_update_cursor();
    }
}

// Get current cursor row
size_t vga_get_cursor_row(void) {
    return vga_row;
}

// Get current cursor column
size_t vga_get_cursor_column(void) {
    return vga_column;
}

// framebuf.c - Framebuffer drawing primitives

#include "../include/framebuf.h"
#include "../include/vbe.h"
#include "../include/string.h"
#include "../include/builtin_font.h"
#include "../include/mm.h"

fb_info_t fb;

int fb_init(uint16_t width, uint16_t height, uint16_t bpp) {
    void* addr = vbe_init(width, height, bpp);
    if (!addr) return -1;

    fb.address   = addr;
    fb.phys_addr = vbe_get_lfb_phys();
    fb.width     = vbe_get_width();
    fb.height    = vbe_get_height();
    fb.bpp       = vbe_get_bpp();
    fb.pitch     = fb.width * (fb.bpp / 8);

    /* Allocate backbuffer for double buffering */
    fb.backbuffer = kmalloc(fb.pitch * fb.height);
    if (!fb.backbuffer) return -1;

    return 0;
}

void fb_putpixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)fb.width || y < 0 || y >= (int)fb.height)
        return;

    if (fb.bpp == 32) {
        uint32_t* p = (uint32_t*)fb.backbuffer + y * fb.width + x;
        *p = color;
    } else if (fb.bpp == 16) {
        // RGB 5-6-5
        uint16_t c16 = ((color >> 8) & 0xF800) | ((color >> 5) & 0x07E0) | ((color >> 3) & 0x001F);
        uint16_t* p = (uint16_t*)fb.backbuffer + y * fb.width + x;
        *p = c16;
    }
}

void fb_fillrect(int x, int y, int w, int h, uint32_t color) {
    // Clip to screen
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > (int)fb.width)  w = fb.width - x;
    if (y + h > (int)fb.height) h = fb.height - y;
    if (w <= 0 || h <= 0) return;

    if (fb.bpp == 32) {
        uint32_t* ptr = (uint32_t*)fb.backbuffer + y * fb.width + x;
        for (int row = 0; row < h; row++) {
            for (int col = 0; col < w; col++) {
                ptr[col] = color;
            }
            ptr += fb.width;
        }
    } else if (fb.bpp == 16) {
        uint16_t c16 = ((color >> 8) & 0xF800) | ((color >> 5) & 0x07E0) | ((color >> 3) & 0x001F);
        uint16_t* ptr = (uint16_t*)fb.backbuffer + y * fb.width + x;
        for (int row = 0; row < h; row++) {
            for (int col = 0; col < w; col++) {
                ptr[col] = c16;
            }
            ptr += fb.width;
        }
    }
}

void fb_hline(int x, int y, int len, uint32_t color) {
    fb_fillrect(x, y, len, 1, color);
}

void fb_vline(int x, int y, int len, uint32_t color) {
    fb_fillrect(x, y, 1, len, color);
}

void fb_drawrect(int x, int y, int w, int h, uint32_t color) {
    fb_hline(x, y, w, color);
    fb_hline(x, y + h - 1, w, color);
    fb_vline(x, y, h, color);
    fb_vline(x + w - 1, y, h, color);
}

// VGA 8x16 font glyph rendering. font8x16 points to 256 * 16 bytes of bitmap data.
// Each character is 16 bytes, each byte is 8 pixels (MSB = leftmost pixel).
// If font8x16 is NULL, uses built-in fallback font.
void fb_drawchar(int x, int y, char ch, uint32_t fg, uint32_t bg, const uint8_t* font8x16) {
    if (!font8x16) font8x16 = builtin_font_get();
    const uint8_t* glyph = font8x16 + (uint8_t)ch * 16;

    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        if (y + row >= (int)fb.height) break;
        if (y + row < 0) continue;

        for (int col = 0; col < 8; col++) {
            int px = x + col;
            if (px >= (int)fb.width) break;
            if (px < 0) continue;
            fb_putpixel(px, y + row, (bits & (0x80 >> col)) ? fg : bg);
        }
    }
}

void fb_drawstring(int x, int y, const char* str, uint32_t fg, uint32_t bg, const uint8_t* font8x16) {
    while (*str) {
        fb_drawchar(x, y, *str, fg, bg, font8x16);
        x += 8;
        str++;
    }
}

void fb_clear(uint32_t color) {
    fb_fillrect(0, 0, fb.width, fb.height, color);
}

void fb_flip(void) {
    memcpy(fb.address, fb.backbuffer, fb.pitch * fb.height);
}
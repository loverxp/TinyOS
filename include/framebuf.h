#ifndef FRAMEBUF_H
#define FRAMEBUF_H

#include "types.h"

/* Framebuffer info */
typedef struct {
    void*   address;       /* Virtual address of framebuffer (LFB) */
    void*   backbuffer;    /* Backbuffer for double buffering */
    uint32_t phys_addr;    /* Physical address */
    uint16_t width;
    uint16_t height;
    uint16_t bpp;          /* Bits per pixel */
    uint16_t pitch;        /* Bytes per scan line */
} fb_info_t;

/* Global framebuffer info */
extern fb_info_t fb;

/* Initialize framebuffer from VBE */
int fb_init(uint16_t width, uint16_t height, uint16_t bpp);

/* Put a pixel (32-bit color, native format) */
void fb_putpixel(int x, int y, uint32_t color);

/* Fill a rectangle */
void fb_fillrect(int x, int y, int w, int h, uint32_t color);

/* Draw a horizontal line */
void fb_hline(int x, int y, int len, uint32_t color);

/* Draw a vertical line */
void fb_vline(int x, int y, int len, uint32_t color);

/* Draw a rectangle outline */
void fb_drawrect(int x, int y, int w, int h, uint32_t color);

/* Draw a character using VGA font (8x16 pixels) at given position.
   Font data must be provided by caller. */
void fb_drawchar(int x, int y, char ch, uint32_t fg, uint32_t bg, const uint8_t* font8x16);

/* Draw a string */
void fb_drawstring(int x, int y, const char* str, uint32_t fg, uint32_t bg, const uint8_t* font8x16);

/* Clear the entire framebuffer to a color */
void fb_clear(uint32_t color);

/* Flip backbuffer to LFB (copy entire backbuffer to screen) */
void fb_flip(void);

/* 32-bit ARGB color helper */
#define RGB(r, g, b)  ((uint32_t)(0xFF000000 | ((r)<<16) | ((g)<<8) | (b)))
#define RGBA(r,g,b,a) ((uint32_t)(((a)<<24) | ((r)<<16) | ((g)<<8) | (b)))

#endif /* FRAMEBUF_H */
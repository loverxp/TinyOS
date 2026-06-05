// wm.c - Simple window manager for TinyOS GUI

#include "../include/window.h"
#include "../include/framebuf.h"
#include "../include/mouse.h"
#include "../include/string.h"
#include "../include/vga.h"     // For font data access
#include "../include/stdio.h"
#include "../include/mm.h"      // For kmalloc/kfree
#include "../include/builtin_font.h"

// Shortcut: WM always uses built-in font for text rendering
#define wm_font()  builtin_font_get()

// External font data saved by VGA driver - use vga_get_font() instead

// Window list
static window_t* windows[WM_MAX_WINDOWS];
static int window_count = 0;
static window_t* focused = NULL;

// Drag state
static int dragging = 0;
static window_t* drag_win = NULL;
static int drag_off_x, drag_off_y;

// Redraw control
static volatile int gui_needs_redraw = 1;  // full redraw (windows changed)
static volatile int gui_cursor_moved = 0;  // cursor-only redraw

// Title bar height (in pixels)
#define TITLEBAR_H 20

// Draw a filled rounded-ish rectangle (simple version, just flat)
static void draw_panel(int x, int y, int w, int h, uint32_t color, uint32_t border) {
    fb_fillrect(x, y, w, h, color);
    fb_drawrect(x, y, w, h, border);
}

void wm_init(void) {
    window_count = 0;
    focused = NULL;
    dragging = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        windows[i] = NULL;
    printf("[WM] Window manager initialized\n");
}

window_t* wm_create_window(int x, int y, int w, int h, const char* title,
    void (*draw)(struct window*), void (*on_key)(struct window*, char))
{
    if (window_count >= WM_MAX_WINDOWS) {
        printf("[WM] Max windows reached\n");
        return NULL;
    }

    // Allocate window
    window_t* win = (window_t*)kmalloc(sizeof(window_t));
    if (!win) return NULL;

    win->x = x;
    win->y = y;
    win->width  = w;
    win->height = h;
    win->flags  = WM_WF_VISIBLE | WM_WF_HAS_TITLEBAR;
    strncpy(win->title, title, sizeof(win->title) - 1);
    win->title[sizeof(win->title) - 1] = '\0';
    win->draw    = draw;
    win->on_key  = on_key;
    win->on_click = NULL;
    win->user_data = NULL;

    // Add to list
    windows[window_count++] = win;
    wm_focus_window(win);

    return win;
}

void wm_close_window(window_t* win) {
    if (!win) return;

    // Remove from list
    int found = 0;
    for (int i = 0; i < window_count; i++) {
        if (windows[i] == win) {
            found = 1;
        }
        if (found && i + 1 < window_count) {
            windows[i] = windows[i + 1];
        }
    }
    if (found) {
        window_count--;
        windows[window_count] = NULL;
    }

    // Adjust focus
    if (focused == win) {
        focused = (window_count > 0) ? windows[window_count - 1] : NULL;
    }

    kfree(win);
}

// ── Cursor drawing ────────────────────────────────────────────────────

#define CURSOR_W 12
#define CURSOR_H 19

// Simple 12x19 arrow cursor bitmap (1 = white pixel)
static const uint8_t cursor_bits[CURSOR_H] = {
    0b10000000,
    0b11000000,
    0b11100000,
    0b11110000,
    0b11111000,
    0b11111100,
    0b11111110,
    0b11111111,
    0b11111111,
    0b11111110,
    0b11111100,
    0b11111000,
    0b10110000,
    0b10011000,
    0b10001100,
    0b10000110,
    0b10000011,
    0b10000000,
    0b10000000,
};

// Saved background pixels under cursor (for restore)
static uint32_t cursor_bg[CURSOR_W * CURSOR_H];
static int cursor_prev_x = -1;
static int cursor_prev_y = -1;

// Save the background pixels at (mx, my) into cursor_bg[]
static void cursor_save_bg(int mx, int my) {
    int idx = 0;
    for (int row = 0; row < CURSOR_H; row++) {
        int py = my + row;
        if (py < 0 || py >= (int)fb.height) { idx += CURSOR_W; continue; }
        for (int col = 0; col < CURSOR_W; col++) {
            int px = mx + col;
            if (px < 0 || px >= (int)fb.width) { idx++; continue; }
            cursor_bg[idx++] = ((uint32_t*)fb.address)[py * fb.width + px];
        }
    }
    cursor_prev_x = mx;
    cursor_prev_y = my;
}

// Restore previously saved background (erases cursor)
static void cursor_restore_bg(void) {
    if (cursor_prev_x < 0 || cursor_prev_y < 0) return;
    int idx = 0;
    for (int row = 0; row < CURSOR_H; row++) {
        int py = cursor_prev_y + row;
        if (py < 0 || py >= (int)fb.height) { idx += CURSOR_W; continue; }
        for (int col = 0; col < CURSOR_W; col++) {
            int px = cursor_prev_x + col;
            if (px < 0 || px >= (int)fb.width) { idx++; continue; }
            ((uint32_t*)fb.address)[py * fb.width + px] = cursor_bg[idx++];
        }
    }
    cursor_prev_x = -1;
    cursor_prev_y = -1;
}

void wm_draw_cursor(void) {
    int mx = mouse_x;
    int my = mouse_y;

    for (int row = 0; row < CURSOR_H; row++) {
        uint8_t bits = cursor_bits[row];
        int py = my + row;
        if (py < 0 || py >= (int)fb.height) continue;
        for (int col = 0; col < CURSOR_W; col++) {
            int px = mx + col;
            if (px < 0 || px >= (int)fb.width) continue;
            if (bits & (0x80 >> col))
                // Write directly to LFB (not backbuffer) — cursor is on top of flipped content
                ((uint32_t*)fb.address)[py * fb.width + px] = RGB(0xFF, 0xFF, 0xFF);
        }
    }
}

// Redraw flag management
int wm_redraw_needed(void) {
    return gui_needs_redraw;
}

void wm_set_redraw(void) {
    gui_needs_redraw = 1;
}

// Call this after full redraw to seed the cursor background
void wm_seed_cursor(void) {
    cursor_save_bg(mouse_x, mouse_y);
    wm_draw_cursor();
    cursor_prev_x = mouse_x;
    cursor_prev_y = mouse_y;
    gui_needs_redraw = 0;
    gui_cursor_moved = 0;
}

// Incremental cursor update: restore old bg, save new bg, draw cursor
void wm_update_cursor(void) {
    cursor_restore_bg();
    cursor_save_bg(mouse_x, mouse_y);
    wm_draw_cursor();
    gui_cursor_moved = 0;
}

int wm_cursor_moved(void) {
    return gui_cursor_moved;
}

void wm_focus_window(window_t* win) {
    if (!win) return;

    // Unfocus all
    for (int i = 0; i < window_count; i++) {
        windows[i]->flags &= ~WM_WF_FOCUSED;
    }

    win->flags |= WM_WF_FOCUSED;
    focused = win;

    // Move to end of list (top z-order)
    int idx = -1;
    for (int i = 0; i < window_count; i++) {
        if (windows[i] == win) { idx = i; break; }
    }
    if (idx >= 0 && idx < window_count - 1) {
        // Shift windows down
        for (int i = idx; i < window_count - 1; i++) {
            windows[i] = windows[i + 1];
        }
        windows[window_count - 1] = win;
    }
}

void wm_move_window(window_t* win, int new_x, int new_y) {
    win->x = new_x;
    win->y = new_y;
}

void wm_resize_window(window_t* win, int new_w, int new_h) {
    if (new_w < 20) new_w = 20;
    if (new_h < 20) new_h = 20;
    win->width  = new_w;
    win->height = new_h;
}

// Get title bar height (0 if no title bar)
static int win_title_h(window_t* win) {
    return (win->flags & WM_WF_HAS_TITLEBAR) ? TITLEBAR_H : 0;
}

void wm_draw_desktop(void) {
    fb_clear(WM_COLOR_DESKTOP);
}

void wm_draw_titlebar(window_t* win) {
    int th = win_title_h(win);
    if (!th) return;

    uint32_t title_bg = (win->flags & WM_WF_FOCUSED) ? WM_COLOR_TITLE_BG : RGB(0x60, 0x60, 0x60);
    uint32_t title_fg = WM_COLOR_TITLE_TEXT;

    // Title bar background
    fb_fillrect(win->x, win->y, win->width, th, title_bg);

    // Bottom border of title bar
    fb_hline(win->x, win->y + th, win->width, WM_COLOR_BORDER);

    // Title text (left aligned, 1px padding)
    int text_x = win->x + 3;
    int text_y = win->y + (th - 16) / 2;  // Center vertically (font is 16px)
    fb_drawstring(text_x, text_y, win->title, title_fg, title_bg, wm_font());

    // Close button (top right)
    int cb_size = th - 4;
    int cb_x = win->x + win->width - cb_size - 2;
    int cb_y = win->y + 2;
    fb_fillrect(cb_x, cb_y, cb_size, cb_size, WM_COLOR_CLOSE_BTN);
    fb_drawrect(cb_x, cb_y, cb_size, cb_size, RGB(0x80, 0x00, 0x00));

    // Draw X in close button
    if (cb_size >= 6) {
        for (int i = 0; i < cb_size - 4; i++) {
            fb_putpixel(cb_x + 2 + i, cb_y + 2 + i, RGB(0xFF, 0xFF, 0xFF));
            fb_putpixel(cb_x + 2 + i, cb_y + cb_size - 3 - i, RGB(0xFF, 0xFF, 0xFF));
        }
    }
}

void wm_fill_client(window_t* win, uint32_t color) {
    int th = win_title_h(win);
    fb_fillrect(win->x, win->y + th, win->width, win->height, color);
}

void wm_redraw(void) {
    // Draw desktop background
    wm_draw_desktop();

    // Draw all visible windows in z-order
    for (int i = 0; i < window_count; i++) {
        window_t* win = windows[i];
        if (!(win->flags & WM_WF_VISIBLE)) continue;

        int th = win_title_h(win);
        int total_h = win->height + th;

        // Draw window border
        fb_drawrect(win->x, win->y, win->width, total_h, WM_COLOR_BORDER);

        // Draw title bar
        wm_draw_titlebar(win);

        // Draw client area background (default)
        fb_fillrect(win->x, win->y + th, win->width, win->height, WM_COLOR_CLIENT_BG);

        // Call window's draw routine
        if (win->draw) {
            win->draw(win);
        }
    }
}

// Check if a point is inside the close button of a window
static int is_in_close_btn(window_t* win, int mx, int my) {
    int th = win_title_h(win);
    if (!th) return 0;
    int cb_size = th - 4;
    int cb_x = win->x + win->width - cb_size - 2;
    int cb_y = win->y + 2;
    return (mx >= cb_x && mx < cb_x + cb_size && my >= cb_y && my < cb_y + cb_size);
}

// Check if a point is in the title bar (but not on close button)
static int is_in_titlebar(window_t* win, int mx, int my) {
    int th = win_title_h(win);
    if (!th) return 0;
    int cb_size = th - 4;
    int cb_x = win->x + win->width - cb_size - 2;
    int in_title_y = (my >= win->y && my < win->y + th);
    int in_title_x = (mx >= win->x && mx < win->x + win->width);
    int in_close = (mx >= cb_x && mx < cb_x + cb_size && my >= win->y + 2 && my < win->y + 2 + cb_size);
    return in_title_y && in_title_x && !in_close;
}

// Check if a point is inside a window's total area (including title bar)
static int is_in_window(window_t* win, int mx, int my) {
    int th = win_title_h(win);
    int total_h = win->height + th;
    return (mx >= win->x && mx < win->x + win->width &&
            my >= win->y && my < win->y + total_h);
}

// Find the topmost window at coordinates (search from end = top)
static window_t* window_at(int mx, int my) {
    for (int i = window_count - 1; i >= 0; i--) {
        if (windows[i]->flags & WM_WF_VISIBLE) {
            if (is_in_window(windows[i], mx, my))
                return windows[i];
        }
    }
    return NULL;
}

window_t* wm_handle_mouse(int mx, int my, uint8_t buttons) {
    static int prev_mx = 0, prev_my = 0;
    static uint8_t prev_buttons = 0;
    static int in_drag = 0;  // tracks if we're in a drag operation

    // Detect simple movement (no button interaction with windows)
    int mouse_moved = (mx != prev_mx || my != prev_my);
    prev_mx = mx; prev_my = my;

    // Left button pressed this frame
    uint8_t pressed = buttons & ~prev_buttons;
    uint8_t released = prev_buttons & ~buttons;
    prev_buttons = buttons;

    if (pressed & 0x01) {  // Left click
        // Check close button of focused window first
        if (focused && is_in_close_btn(focused, mx, my)) {
            window_t* to_close = focused;
            wm_close_window(to_close);
            return NULL;
        }

        // Find window at click position
        window_t* hit = window_at(mx, my);
        if (hit) {
            wm_focus_window(hit);

            // Start drag if in title bar
            if (is_in_titlebar(hit, mx, my)) {
                dragging = 1;
                drag_win = hit;
                drag_off_x = mx - hit->x;
                drag_off_y = my - hit->y;
            }

            // Notify window of click
            if (hit->on_click) {
                int th = win_title_h(hit);
                hit->on_click(hit, mx - hit->x, my - hit->y - th, 1);
            }

            return hit;
        }
    }

    if (released & 0x01) {
        dragging = 0;
        drag_win = NULL;
    }

    // Handle dragging
    if (dragging && drag_win && (buttons & 0x01)) {
        int new_x = mx - drag_off_x;
        int new_y = my - drag_off_y;
        // Clamp to screen edges
        if (new_x < 0) new_x = 0;
        if (new_y < 0) new_y = 0;
        if (new_x + drag_win->width  > (int)fb.width)  new_x = fb.width  - drag_win->width;
        if (new_y + drag_win->height + win_title_h(drag_win) > (int)fb.height)
            new_y = fb.height - drag_win->height - win_title_h(drag_win);
        wm_move_window(drag_win, new_x, new_y);
        gui_needs_redraw = 1;   // window moved → full redraw
        in_drag = 1;
    }

    // Set redraw flags
    if (gui_needs_redraw) {
        // already set above (window close, focus, drag)
    } else if (released & 0x01) {
        gui_needs_redraw = 1;   // button release → full redraw (clean up drag artifacts)
    } else if (mouse_moved && !in_drag) {
        gui_cursor_moved = 1;   // simple movement → cursor-only update
    }

    if (!(buttons & 0x01))
        in_drag = 0;

    return NULL;
}

void wm_handle_key(char c) {
    if (focused && focused->on_key) {
        focused->on_key(focused, c);
    }
}

window_t* wm_get_focused(void) {
    return focused;
}

void wm_debug(void) {
    printf("[WM] Windows: %d, Focus: %s\n", window_count, focused ? focused->title : "none");
    for (int i = 0; i < window_count; i++) {
        printf("  [%d] %s @ (%d,%d) %dx%d\n", i, windows[i]->title,
            windows[i]->x, windows[i]->y, windows[i]->width, windows[i]->height);
    }
}
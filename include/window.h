#ifndef WINDOW_H
#define WINDOW_H

#include "types.h"

/* Maximum number of windows */
#define WM_MAX_WINDOWS 16

/* Window flags */
#define WM_WF_VISIBLE      (1 << 0)
#define WM_WF_HAS_TITLEBAR (1 << 1)
#define WM_WF_FOCUSED      (1 << 2)

/* Window colors (default theme) */
#define WM_COLOR_TITLE_BG      RGB(0x00, 0x40, 0x80)
#define WM_COLOR_TITLE_TEXT    RGB(0xFF, 0xFF, 0xFF)
#define WM_COLOR_CLIENT_BG     RGB(0xE0, 0xE0, 0xE0)
#define WM_COLOR_BORDER        RGB(0x00, 0x00, 0x00)
#define WM_COLOR_DESKTOP       RGB(0x00, 0x80, 0x40)
#define WM_COLOR_CLOSE_BTN     RGB(0xC0, 0x00, 0x00)

/* Window structure */
typedef struct window {
    int x, y;           /* Position */
    int width, height;  /* Client area dimensions */
    int flags;          /* WM_WF_* flags */
    char title[32];     /* Window title */
    void (*draw)(struct window* win);           /* Draw callback */
    void (*on_key)(struct window* win, char c); /* Key event callback */
    void (*on_click)(struct window* win, int mx, int my, int btn); /* Click callback */
    void* user_data;    /* Per-window user data */
} window_t;

/* Initialize the window manager */
void wm_init(void);

/* Create a window */
window_t* wm_create_window(int x, int y, int w, int h, const char* title,
    void (*draw)(struct window*), void (*on_key)(struct window*, char));

/* Close (remove) a window */
void wm_close_window(window_t* win);

/* Bring a window to the top (focus) */
void wm_focus_window(window_t* win);

/* Move window to new position */
void wm_move_window(window_t* win, int new_x, int new_y);

/* Resize window */
void wm_resize_window(window_t* win, int new_w, int new_h);

/* Redraw all windows to the framebuffer */
void wm_redraw(void);

/* Handle mouse event: update cursor state, dispatch to windows.
   Returns the window clicked on (if any), or NULL. */
window_t* wm_handle_mouse(int mx, int my, uint8_t buttons);

/* Dispatch keyboard event to focused window */
void wm_handle_key(char c);

/* Get the currently focused window */
window_t* wm_get_focused(void);

/* Draw desktop background */
void wm_draw_desktop(void);

/* Draw title bar for a window */
void wm_draw_titlebar(window_t* win);

/* Window drawing helper: fill client area */
void wm_fill_client(window_t* win, uint32_t color);

/* Debug: print window list */
void wm_debug(void);

/* Draw mouse cursor at current mouse_x/mouse_y */
void wm_draw_cursor(void);

/* Seed cursor bg after full redraw, then draw cursor */
void wm_seed_cursor(void);

/* Incremental cursor update (save/restore bg) — no full redraw */
void wm_update_cursor(void);

/* Check if cursor-only redraw is needed */
int wm_cursor_moved(void);

/* Check if redraw is needed (mouse/timer-driven) */
int wm_redraw_needed(void);

/* Force redraw on next cycle */
void wm_set_redraw(void);

#endif /* WINDOW_H */
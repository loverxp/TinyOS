// snake.c - Snake game for TinyOS
// Supports two modes: text mode (VGA 80x25) and graphics mode (future VGA Mode 13h)
//
// Arrow keys to steer, Q or ESC to quit when alive, any key when game over.
//
// The game temporarily takes over keyboard input and registers a timer
// callback for game ticks, then restores everything on exit.

#include "snake.h"
#include "../include/vga.h"
#include "../include/keyboard.h"
#include "../include/timer.h"
#include "../include/stdio.h"
#include "../include/types.h"
#include "../include/io.h"

// ── Serial debug output ────────────────────────────────────────────

static void serial_write(char c) {
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, c);
}

static void serial_string(const char* s) {
    while (*s) serial_write(*s++);
}

static void serial_hex(uint8_t n) {
    char hex[] = "0123456789ABCDEF";
    serial_write(hex[n >> 4]);
    serial_write(hex[n & 0xF]);
}

// ── Game constants ────────────────────────────────────────────────

#define GAME_W   78            // playfield width in characters
#define GAME_H   22            // playfield height in characters
#define GAME_X0  1             // left border X
#define GAME_Y0  2             // top border Y
#define MAX_LEN  (GAME_W * GAME_H)

#define TICK_INTERVAL  10     // move snake every 10 ticks (200 ms @ 50 Hz)

// Colors (text mode)
#define COL_SNAKE   vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK)
#define COL_HEAD    vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK)
#define COL_FOOD    vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK)
#define COL_BORDER  vga_entry_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK)
#define COL_TEXT    vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK)
#define COL_GAMEOVER vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK)
#define COL_SCORE   vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK)

// Characters used in text mode
#define CH_WALL_H   '-'
#define CH_WALL_V   '|'
#define CH_CORNER   '+'
#define CH_SNAKE    '#'    // 0xDB full block would look nicer but # is safe
#define CH_HEAD     '@'
#define CH_FOOD     '$'
#define CH_EMPTY    ' '

// ── Scancode constants ────────────────────────────────────────────

#define SC_ESC      0x01
#define SC_Q        0x10
#define SC_W        0x11
#define SC_E        0x12
#define SC_A        0x1E
#define SC_S        0x1F
#define SC_D        0x20
#define SC_UP       0x48
#define SC_DOWN     0x50
#define SC_LEFT     0x4B
#define SC_RIGHT    0x4D

// ── Direction enum ────────────────────────────────────────────────

enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };

// ── Position type ─────────────────────────────────────────────────

typedef struct { int x, y; } pos_t;

// Game state (volatile because accessed from interrupt callbacks)
static pos_t     snake[MAX_LEN];
static volatile int       snake_len;
static volatile int       dir;          // current movement direction
static volatile int       next_dir;     // buffered direction (debounce multiple keys)
static volatile int       food_x, food_y;
static volatile int       score;
static volatile int       game_over;
static volatile int       running;      // 1 while game is active
static int       game_mode;    // SNAKE_MODE_TEXT or SNAKE_MODE_GRAPHICS
static volatile uint32_t  last_move_tick;
static int       old_tail_x, old_tail_y;  // position to clear in next render

// Saved state (for restore on exit)
static uint16_t  saved_screen[VGA_WIDTH * VGA_HEIGHT];
static int       saved_cursor_row;
static int       saved_cursor_col;

// Direct VGA buffer pointer (text mode)
static volatile uint16_t* const vga = (uint16_t*)0xB8000;

// ── Helpers ───────────────────────────────────────────────────────

static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

static void put_cell(int x, int y, char c, uint8_t color) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT)
        vga[y * VGA_WIDTH + x] = vga_entry(c, color);
}

// ── Random food placement ─────────────────────────────────────────

// Simple pseudo-random using timer ticks as seed
static int rand_range(int min, int max) {
    static uint32_t seed = 0;
    if (seed == 0) seed = timer_get_ticks() + 1;
    seed = seed * 1103515245 + 12345;
    return min + (int)((seed >> 16) % (uint32_t)(max - min + 1));
}

static int is_snake_cell(int x, int y) {
    for (int i = 0; i < snake_len; i++)
        if (snake[i].x == x && snake[i].y == y) return 1;
    return 0;
}

static void spawn_food(void) {
    // Try up to 1000 times to find an empty cell
    for (int attempt = 0; attempt < 1000; attempt++) {
        int x = rand_range(GAME_X0, GAME_X0 + GAME_W - 1);
        int y = rand_range(GAME_Y0, GAME_Y0 + GAME_H - 1);
        if (!is_snake_cell(x, y)) {
            food_x = x;
            food_y = y;
            return;
        }
    }
    // Fallback: scan linearly
    for (int y = GAME_Y0; y < GAME_Y0 + GAME_H; y++)
        for (int x = GAME_X0; x < GAME_X0 + GAME_W; x++)
            if (!is_snake_cell(x, y)) { food_x = x; food_y = y; return; }
}

// ── Rendering (text mode) ─────────────────────────────────────────

static void clear_playfield(void) {
    for (int y = GAME_Y0; y < GAME_Y0 + GAME_H; y++)
        for (int x = GAME_X0; x < GAME_X0 + GAME_W; x++)
            put_cell(x, y, CH_EMPTY, COL_TEXT);
}

static void draw_border(void) {
    int x, y;
    // Top / bottom
    for (x = GAME_X0; x < GAME_X0 + GAME_W; x++) {
        put_cell(x, GAME_Y0 - 1, CH_WALL_H, COL_BORDER);
        put_cell(x, GAME_Y0 + GAME_H, CH_WALL_H, COL_BORDER);
    }
    // Left / right
    for (y = GAME_Y0; y < GAME_Y0 + GAME_H; y++) {
        put_cell(GAME_X0 - 1, y, CH_WALL_V, COL_BORDER);
        put_cell(GAME_X0 + GAME_W, y, CH_WALL_V, COL_BORDER);
    }
    // Corners
    put_cell(GAME_X0 - 1, GAME_Y0 - 1, CH_CORNER, COL_BORDER);
    put_cell(GAME_X0 + GAME_W, GAME_Y0 - 1, CH_CORNER, COL_BORDER);
    put_cell(GAME_X0 - 1, GAME_Y0 + GAME_H, CH_CORNER, COL_BORDER);
    put_cell(GAME_X0 + GAME_W, GAME_Y0 + GAME_H, CH_CORNER, COL_BORDER);
}

static void draw_snake(void) {
    for (int i = 0; i < snake_len; i++) {
        char c = (i == snake_len - 1) ? CH_HEAD : CH_SNAKE;
        uint8_t col = (i == snake_len - 1) ? COL_HEAD : COL_SNAKE;
        put_cell(snake[i].x, snake[i].y, c, col);
    }
}

static void draw_food(void) {
    put_cell(food_x, food_y, CH_FOOD, COL_FOOD);
}

static void draw_header(void) {
    // Title bar with score
    char buf[VGA_WIDTH + 1];
    int p = 0;
    buf[p++] = ' ';
    const char* title = "SNAKE GAME";
    for (int i = 0; title[i]; i++) buf[p++] = title[i];
    buf[p++] = ' ';
    // Score
    const char* score_label = "Score: ";
    for (int i = 0; score_label[i]; i++) buf[p++] = score_label[i];
    // Convert score to string
    char score_str[12];
    int si = 0, s = score;
    if (s == 0) score_str[si++] = '0';
    else while (s > 0) { score_str[si++] = '0' + (s % 10); s /= 10; }
    for (int i = si - 1; i >= 0; i--) buf[p++] = score_str[i];
    buf[p++] = ' ';
    // Controls hint
    const char* hint = "WASD/Arrow:Move  Q/ESC:Quit";
    for (int i = 0; hint[i]; i++) buf[p++] = hint[i];
    // Pad rest with spaces
    while (p < VGA_WIDTH) buf[p++] = ' ';
    buf[VGA_WIDTH] = '\0';
    // Write header line
    for (int i = 0; i < VGA_WIDTH; i++)
        put_cell(i, 0, buf[i], COL_SCORE);
}

static void draw_game_over(void) {
    // Show GAME OVER message centered in playfield
    const char* msg = "GAME OVER!";
    const char* score_msg = "Final Score: ";
    char score_str[12];
    int si = 0, s = score;
    if (s == 0) score_str[si++] = '0';
    else while (s > 0) { score_str[si++] = '0' + (s % 10); s /= 10; }
    score_str[si] = '\0';

    int msg_x = GAME_X0 + (GAME_W - 10) / 2;
    int msg_y = GAME_Y0 + GAME_H / 2 - 1;
    for (int i = 0; msg[i]; i++)
        put_cell(msg_x + i, msg_y, msg[i], COL_GAMEOVER);

    int sc_x = GAME_X0 + (GAME_W - 13) / 2;
    int sc_y = GAME_Y0 + GAME_H / 2 + 1;
    for (int i = 0; score_msg[i]; i++)
        put_cell(sc_x + i, sc_y, score_msg[i], COL_TEXT);
    for (int i = 0; score_str[i]; i++)
        put_cell(sc_x + 13 + i, sc_y, score_str[i], COL_SCORE);
}

// ── Graphics mode rendering (stub for future) ─────────────────────

static void gfx_clear(void) {
    // TODO: VGA Mode 13h (320x200x256)
    // For now, fall back to text mode message
    vga_clear_screen(VGA_COLOR_BLACK);
    vga_set_cursor(10, 10);
    printf("Snake graphics mode - not yet implemented");
}

static void gfx_draw_border(void) { /* TODO */ }
static void gfx_draw_snake(void)  { /* TODO */ }
static void gfx_draw_food(void)   { /* TODO */ }
static void gfx_draw_header(void) { /* TODO */ }
static void gfx_draw_game_over(void) { /* TODO */ }

// ── Render dispatch ───────────────────────────────────────────────

static void render_all(void) {
    if (game_mode == SNAKE_MODE_TEXT) {
        // Clear the old tail position (avoids trail artifacts)
        if (old_tail_x >= 0 && old_tail_y >= 0) {
            put_cell(old_tail_x, old_tail_y, CH_EMPTY, COL_TEXT);
            old_tail_x = -1;
            old_tail_y = -1;
        }
        draw_header();
        draw_border();
        draw_food();
        draw_snake();
        if (game_over) draw_game_over();
    } else {
        gfx_clear();
        gfx_draw_border();
        gfx_draw_snake();
        gfx_draw_food();
        gfx_draw_header();
        if (game_over) gfx_draw_game_over();
    }
}

// ── Game logic ────────────────────────────────────────────────────

static void init_snake(void) {
    snake_len = 3;
    int cx = GAME_X0 + GAME_W / 2;
    int cy = GAME_Y0 + GAME_H / 2;
    snake[0].x = cx - 2; snake[0].y = cy;
    snake[1].x = cx - 1; snake[1].y = cy;
    snake[2].x = cx;     snake[2].y = cy;
    dir = DIR_RIGHT;
    next_dir = DIR_RIGHT;
    score = 0;
    game_over = 0;
    old_tail_x = -1;
    old_tail_y = -1;
}

static void move_snake(void) {
    if (game_over) return;

    // Commit buffered direction (prevent reversing)
    if ((dir == DIR_UP    && next_dir != DIR_DOWN)  ||
        (dir == DIR_DOWN  && next_dir != DIR_UP)    ||
        (dir == DIR_LEFT  && next_dir != DIR_RIGHT) ||
        (dir == DIR_RIGHT && next_dir != DIR_LEFT)) {
        dir = next_dir;
    }

    // Calculate new head position
    pos_t head = snake[snake_len - 1];
    pos_t new_head;
    new_head.x = head.x;
    new_head.y = head.y;

    switch (dir) {
        case DIR_UP:    new_head.y--; break;
        case DIR_DOWN:  new_head.y++; break;
        case DIR_LEFT:  new_head.x--; break;
        case DIR_RIGHT: new_head.x++; break;
    }

    // Check wall collision
    if (new_head.x < GAME_X0 || new_head.x >= GAME_X0 + GAME_W ||
        new_head.y < GAME_Y0 || new_head.y >= GAME_Y0 + GAME_H) {
        game_over = 1;
        return;
    }

    // Check food collision (before self collision: food cell is "empty")
    int ate = (new_head.x == food_x && new_head.y == food_y);

    // Check self collision (skip tail if not eating - it will move away)
    int check_limit = snake_len - (ate ? 0 : 1);
    for (int i = 0; i < check_limit; i++) {
        if (snake[i].x == new_head.x && snake[i].y == new_head.y) {
            game_over = 1;
            return;
        }
    }

    // Move: add new head
    if (snake_len < MAX_LEN) {
        snake[snake_len] = new_head;
        snake_len++;
    }

    // Remove tail if not eating
    if (!ate) {
        // Save old tail position for clearing
        old_tail_x = snake[0].x;
        old_tail_y = snake[0].y;
        for (int i = 0; i < snake_len - 1; i++)
            snake[i] = snake[i + 1];
        snake_len--;
    } else {
        // Ate food: tail stays, snake grows
        old_tail_x = -1;  // no old tail to clear
        old_tail_y = -1;
        score++;
        spawn_food();
    }
}

// ── Callbacks ─────────────────────────────────────────────────────

static void snake_raw_cb(uint8_t scancode, uint8_t extended) {
    // Debug: log key press to serial
    serial_string("[SNAKE] Key: 0x");
    serial_hex(scancode);
    if (extended) serial_string(" ext");
    serial_string("\n");

    if (game_over) {
        serial_string("[SNAKE] Game over, exiting\n");
        running = 0;
        return;
    }

    if (extended) {
        // Extended scancodes = arrow keys
        switch (scancode) {
            case SC_UP:    if (dir != DIR_DOWN)  next_dir = DIR_UP;    break;
            case SC_DOWN:  if (dir != DIR_UP)    next_dir = DIR_DOWN;  break;
            case SC_LEFT:  if (dir != DIR_RIGHT) next_dir = DIR_LEFT;  break;
            case SC_RIGHT: if (dir != DIR_LEFT)  next_dir = DIR_RIGHT; break;
        }
    } else {
        // Normal keys: WASD for direction, Q/ESC to quit
        switch (scancode) {
            case SC_W: if (dir != DIR_DOWN)  next_dir = DIR_UP;    break;
            case SC_S: if (dir != DIR_UP)    next_dir = DIR_DOWN;  break;
            case SC_A: if (dir != DIR_RIGHT) next_dir = DIR_LEFT;  break;
            case SC_D: if (dir != DIR_LEFT)  next_dir = DIR_RIGHT; break;
            case SC_Q: case SC_ESC: 
                serial_string("[SNAKE] Quit key\n");
                running = 0; 
                break;
        }
    }
}

// Timer tick: advance game state periodically
static void snake_tick(void) {
    if (!running) return;

    uint32_t now = timer_get_ticks();
    if (now - last_move_tick >= TICK_INTERVAL) {
        last_move_tick = now;
        serial_string("[SNAKE] Tick dir=");
        serial_hex((uint8_t)dir);
        serial_string(" nxt=");
        serial_hex((uint8_t)next_dir);
        serial_string("\n");
        move_snake();
        render_all();
    }
}

// ── Screen save / restore ─────────────────────────────────────────

static void save_screen(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        saved_screen[i] = vga[i];
    saved_cursor_row = vga_get_cursor_row();
    saved_cursor_col = vga_get_cursor_column();
}

static void restore_screen(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga[i] = saved_screen[i];
    vga_set_cursor(saved_cursor_row, saved_cursor_col);
}

// ── Public API ────────────────────────────────────────────────────

void snake_start(int mode) {
    // Validate mode
    if (mode != SNAKE_MODE_TEXT && mode != SNAKE_MODE_GRAPHICS)
        mode = SNAKE_MODE_TEXT;

    game_mode = mode;

    // Save current state
    save_screen();

    // Initialize game state
    init_snake();
    last_move_tick = timer_get_ticks();
    running = 1;

    if (game_mode == SNAKE_MODE_TEXT) {
        // Clear screen
        vga_clear_screen(VGA_COLOR_BLACK);

        // Initial render
        spawn_food();
        render_all();

        // Register raw callback for input
        keyboard_register_raw_callback(snake_raw_cb);
        // Temporarily set char callback to NULL to prevent shell from processing input
        keyboard_register_char_callback(NULL);
        // Register timer tick callback for game movement
        timer_register_tick_callback(snake_tick);
        
        // Re-enable keyboard IRQ (in case it was somehow masked)
        outb(0x21, inb(0x21) & ~(1 << 1));

        // The game runs via interrupts: timer tick + keyboard callbacks.
        // Main loop halts waiting for interrupts (sti needed since we're
        // called from within an interrupt handler where IF=0).
        while (running) {
            asm volatile("sti\n\t"
                         "hlt");
        }

        // Restore shell callback
        keyboard_register_raw_callback(NULL);
        timer_register_tick_callback(NULL);
        extern void shell_init(void);
        shell_init();

        // Restore screen
        restore_screen();

    } else {
        // Graphics mode (future)
        gfx_clear();

        keyboard_register_raw_callback(snake_raw_cb);
        keyboard_register_char_callback(NULL);

        while (running) {
            asm volatile("sti\n\t"
                         "hlt");
        }

        keyboard_register_raw_callback(NULL);
        extern void shell_init(void);
        shell_init();
    }
}
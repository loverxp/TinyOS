/* gfxsnake.c - Pixel-mode Snake game for TinyOS (VGA Mode 13h)
 * Runs as an independent user program in Ring 3.
 * Uses syscalls for timer, keyboard, and video mode.
 */

#include "../include/types.h"

// ══ Syscall wrappers ════════════════════════════════════════

static void sys_exit(int code) {
    asm volatile("mov $0, %%eax; int $0x80" : : "b"(code) : "eax", "memory");
}

static uint32_t sys_get_ticks(void) {
    uint32_t ticks;
    asm volatile("mov $2, %%eax; int $0x80" : "=a"(ticks) : : "memory");
    return ticks;
}

static uint32_t sys_read_key(void) {
    uint32_t key;
    asm volatile("mov $3, %%eax; int $0x80" : "=a"(key) : : "memory"); 
    return key;
}

static void sys_set_video_mode(int mode) {
    asm volatile("mov $4, %%eax; int $0x80" : : "b"(mode) : "eax", "ecx", "edx", "memory");
}

static void sys_clear_keybuf(void) {
    asm volatile("mov $5, %%eax; int $0x80" : : : "eax", "memory");    
}

static void sys_debug(const char* msg) {
    asm volatile("mov $6, %%eax; int $0x80" : : "b"(msg) : "eax", "memory");
}

// ══ VGA Mode 13h direct access ═════════════════════════════
// IOPL=3 allows in/out, paging maps 0xA0000 as user-accessible.

#define SCREEN_W    320
#define SCREEN_H    200
#define VRAM_ADDR   0xA0000

static volatile uint8_t* const vram = (uint8_t*)VRAM_ADDR;

// ══ Game constants ══════════════════════════════════════════

#define CELL_SIZE   4       // each grid cell is 4x4 pixels
#define GRID_W      (SCREEN_W / CELL_SIZE)   // 80
#define GRID_H      ((SCREEN_H - 20) / CELL_SIZE)  // 45 (reserve 20px for status)
#define GRID_X0     0
#define GRID_Y0     5       // start below status bar
#define MAX_LEN     (GRID_W * GRID_H)

#define COL_BLACK      0
#define COL_DARKGREEN  2
#define COL_GREEN      10
#define COL_RED        12
#define COL_YELLOW     14
#define COL_WHITE      15
#define COL_CYAN       11
#define COL_BROWN      6
#define COL_DARKGRAY   8
#define COL_ORANGE     13

enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT };
enum { KEY_NONE, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_QUIT };    

typedef struct { int x, y; } pos_t;

// ══ Game state ══════════════════════════════════════════════

static pos_t snake[MAX_LEN];
static int snake_len;
static int dir;
static int next_dir;
static int food_x, food_y;
static int score;
static int game_over;
static int running;
static uint32_t last_move_tick;

// ══ Helpers ═════════════════════════════════════════════════

static void draw_pixel(int x, int y, uint8_t color) {
    if (x >= 0 && x < SCREEN_W && y >= 0 && y < SCREEN_H)
        vram[y * SCREEN_W + x] = color;
}

static void draw_cell(int gx, int gy, uint8_t color) {
    int px = gx * CELL_SIZE;
    int py = gy * CELL_SIZE;
    for (int dy = 0; dy < CELL_SIZE; dy++)
        for (int dx = 0; dx < CELL_SIZE; dx++)
            vram[(py + dy) * SCREEN_W + (px + dx)] = color;
}

static void clear_screen(uint8_t color) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++)
        vram[i] = color;
}

// ══ Random ══════════════════════════════════════════════════

static int rand_range(int min, int max) {
    static uint32_t seed = 0;
    if (seed == 0) seed = sys_get_ticks() + 1;
    seed = seed * 1103515245 + 12345;
    return min + (int)((seed >> 16) % (uint32_t)(max - min + 1));      
}

static int is_snake_cell(int x, int y) {
    for (int i = 0; i < snake_len; i++)
        if (snake[i].x == x && snake[i].y == y) return 1;
    return 0;
}

static void spawn_food(void) {
    for (int attempt = 0; attempt < 1000; attempt++) {
        int x = rand_range(GRID_X0, GRID_X0 + GRID_W - 1);
        int y = rand_range(GRID_Y0, GRID_Y0 + GRID_H - 1);
        if (!is_snake_cell(x, y)) {
            food_x = x;
            food_y = y;
            return;
        }
    }
    // Fallback scan
    for (int y = GRID_Y0; y < GRID_Y0 + GRID_H; y++)
        for (int x = GRID_X0; x < GRID_X0 + GRID_W; x++)
            if (!is_snake_cell(x, y)) { food_x = x; food_y = y; return; }
}

// ══ Digit drawing (pixel font: 3x5 digits 0-9) ═════════════

// Each digit defined as 3 columns x 5 rows of bits (lsb = top)        
static const uint16_t digit_data[10] = {
    0b11101010101010111,  // 0
    0b01001001001001001,  // 1
    0b11100100111010011,  // 2
    0b11100100111000111,  // 3
    0b10110111100100100,  // 4
    0b11110011110000111,  // 5
    0b11110011110110111,  // 6
    0b11100100100100100,  // 7
    0b11110111110110111,  // 8
    0b11110111100100111,  // 9
};

static void draw_digit(int px, int py, int d, uint8_t color) {
    if (d < 0 || d > 9) return;
    uint16_t bits = digit_data[d];
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            if (bits & (1 << (row * 3 + col)))
                draw_pixel(px + col, py + row, color);
        }
    }
}

static void draw_number(int px, int py, int n, uint8_t color) {        
    if (n == 0) {
        draw_digit(px, py, 0, color);
        return;
    }
    char buf[8];
    int i = 0;
    while (n > 0 && i < 8) {
        buf[i++] = '0' + (n % 10);
        n /= 10;
    }
    for (int j = i - 1; j >= 0; j--) {
        draw_digit(px, py, buf[j] - '0', color);
        px += 4;  // 3 pixels + 1 gap
    }
}

// ══ Drawing ═════════════════════════════════════════════════

static void draw_border(void) {
    int px0 = GRID_X0 * CELL_SIZE;
    int py0 = GRID_Y0 * CELL_SIZE;
    int px1 = (GRID_X0 + GRID_W) * CELL_SIZE - 1;
    int py1 = (GRID_Y0 + GRID_H) * CELL_SIZE - 1;
    for (int x = px0; x <= px1; x++) {
        draw_pixel(x, py0, COL_CYAN);
        draw_pixel(x, py1, COL_CYAN);
    }
    for (int y = py0; y <= py1; y++) {
        draw_pixel(px0, y, COL_CYAN);
        draw_pixel(px1, y, COL_CYAN);
    }
}

static void draw_snake(void) {
    for (int i = 0; i < snake_len; i++) {
        uint8_t color = (i == snake_len - 1) ? COL_GREEN : COL_DARKGREEN;
        draw_cell(snake[i].x, snake[i].y, color);
    }
}

static void draw_food(void) {
    // Draw food as a blinking red square with yellow dot in center    
    int px = food_x * CELL_SIZE;
    int py = food_y * CELL_SIZE;
    for (int dy = 0; dy < CELL_SIZE; dy++)
        for (int dx = 0; dx < CELL_SIZE; dx++)
            vram[(py + dy) * SCREEN_W + (px + dx)] = COL_RED;
    // Yellow center dot
    draw_pixel(px + 1, py + 1, COL_YELLOW);
    draw_pixel(px + 2, py + 1, COL_YELLOW);
    draw_pixel(px + 1, py + 2, COL_YELLOW);
    draw_pixel(px + 2, py + 2, COL_YELLOW);
}

static void draw_status(void) {
    // Clear status bar area
    for (int y = 0; y < 20; y++)
        for (int x = 0; x < SCREEN_W; x++)
            vram[y * SCREEN_W + x] = COL_DARKGRAY;

    // Title "SNAKE" in large pixel letters
    uint8_t c = COL_WHITE;
    // S
    int sy = 3;
    draw_pixel(5, sy, c); draw_pixel(6, sy, c); draw_pixel(7, sy, c);  
    draw_pixel(5, sy+1, c);
    draw_pixel(5, sy+2, c); draw_pixel(6, sy+2, c); draw_pixel(7, sy+2, c);
    draw_pixel(7, sy+3, c);
    draw_pixel(5, sy+4, c); draw_pixel(6, sy+4, c); draw_pixel(7, sy+4, c);
    // Score label
    c = COL_YELLOW;
    draw_pixel(80, 3, c); draw_pixel(81, 3, c); draw_pixel(82, 3, c);  // S
    draw_pixel(80, 4, c); draw_pixel(84, 4, c);
    draw_pixel(80, 5, c); draw_pixel(81, 5, c); draw_pixel(82, 5, c); draw_pixel(83, 5, c); draw_pixel(84, 5, c);
    draw_pixel(80, 6, c); draw_pixel(84, 6, c);
    draw_pixel(80, 7, c); draw_pixel(84, 7, c);
    // Score value
    draw_number(90, 3, score, COL_WHITE);
    // Controls hint
    c = COL_WHITE;
    int hx = 180;
    draw_pixel(hx, 9, c); draw_pixel(hx+1, 9, c); // arrow up indicator
    draw_pixel(hx, 14, c); draw_pixel(hx+1, 14, c);
    // Q/ESC hint at far right
    c = COL_DARKGRAY + 8; // light gray
}

static void draw_game_over(void) {
    // Dark overlay
    for (int y = SCREEN_H / 2 - 24; y < SCREEN_H / 2 + 24; y++)        
        for (int x = SCREEN_W / 2 - 80; x < SCREEN_W / 2 + 80; x++)    
            vram[y * SCREEN_W + x] = COL_BLACK;
    // "GAME OVER" text made of blocks
    int ox = SCREEN_W / 2 - 60;
    int oy = SCREEN_H / 2 - 8;
    uint8_t col = COL_RED;

    // G
    for (int row = 0; row < 5; row++) draw_pixel(ox+1, oy+row, col);   
    draw_pixel(ox+2, oy, col); draw_pixel(ox+3, oy, col);
    draw_pixel(ox+3, oy+2, col); draw_pixel(ox+3, oy+3, col);
    draw_pixel(ox+2, oy+3, col);
    // A
    int ax = ox + 8;
    for (int row = 0; row < 5; row++) { draw_pixel(ax, oy+row, col); draw_pixel(ax+3, oy+row, col); }
    for (int col2 = 0; col2 < 4; col2++) draw_pixel(ax+col2, oy, col); 
    for (int col2 = 0; col2 < 4; col2++) draw_pixel(ax+col2, oy+2, col);
    // M
    int mx = ax + 8;
    for (int row = 0; row < 5; row++) { draw_pixel(mx, oy+row, col); draw_pixel(mx+4, oy+row, col); }
    draw_pixel(mx+1, oy+1, col); draw_pixel(mx+2, oy+2, col); draw_pixel(mx+3, oy+1, col);
    // E
    int ex = mx + 8;
    for (int row = 0; row < 5; row++) draw_pixel(ex, oy+row, col);     
    for (int col2 = 0; col2 < 4; col2++) { draw_pixel(ex+col2, oy, col); draw_pixel(ex+col2, oy+2, col); draw_pixel(ex+col2, oy+4, col); }    

    // O
    ox = SCREEN_W / 2 + 16;
    oy = SCREEN_H / 2 + 8;
    for (int row = 0; row < 5; row++) { draw_pixel(ox, oy+row, col); draw_pixel(ox+3, oy+row, col); }
    for (int col2 = 0; col2 < 4; col2++) { draw_pixel(ox+col2, oy, col); draw_pixel(ox+col2, oy+4, col); }
    // V
    int vx = ox + 8;
    for (int row = 0; row < 5; row++) { draw_pixel(vx, oy+row, col); draw_pixel(vx+3, oy+row, col); }
    draw_pixel(vx+1, oy+3, col); draw_pixel(vx+2, oy+3, col);
    // E
    int ey2 = vx + 8;
    for (int row = 0; row < 5; row++) draw_pixel(ey2, oy+row, col);    
    for (int col2 = 0; col2 < 4; col2++) { draw_pixel(ey2+col2, oy, col); draw_pixel(ey2+col2, oy+2, col); draw_pixel(ey2+col2, oy+4, col); } 
    // R
    int rx = ey2 + 8;
    for (int row = 0; row < 5; row++) draw_pixel(rx, oy+row, col);     
    draw_pixel(rx+1, oy, col); draw_pixel(rx+2, oy, col); draw_pixel(rx+3, oy, col);
    draw_pixel(rx+1, oy+2, col); draw_pixel(rx+2, oy+2, col); draw_pixel(rx+3, oy+2, col);
    draw_pixel(rx+1, oy+3, col); draw_pixel(rx+3, oy+4, col);

    // Draw final score
    int score_x = SCREEN_W / 2 - 20;
    int score_y = SCREEN_H / 2 + 24;
    // "SCORE:"
    uint8_t score_col = COL_YELLOW;
    draw_pixel(score_x, score_y, score_col);   // S
    draw_pixel(score_x+2, score_y, score_col); // C
    draw_pixel(score_x+4, score_y, score_col); // O
    draw_pixel(score_x+6, score_y, score_col); // R
    draw_pixel(score_x+8, score_y, score_col); // E
    draw_number(score_x + 24, score_y, score, COL_WHITE);
}

static void render_all(void) {
    clear_screen(COL_BLACK);
    draw_status();
    draw_border();
    draw_food();
    draw_snake();
    if (game_over) draw_game_over();
}

// ══ Input ═══════════════════════════════════════════════════

static int read_input(void) {
    uint32_t key = sys_read_key();
    if (key == 0) return KEY_NONE;

    uint8_t sc = key & 0x7F;
    uint8_t ext = (key & 0x80) ? 1 : 0;

    if (ext) {
        switch (sc) {
            case 0x48: return KEY_UP;
            case 0x50: return KEY_DOWN;
            case 0x4B: return KEY_LEFT;
            case 0x4D: return KEY_RIGHT;
        }
    } else {
        switch (sc) {
            case 0x11: return KEY_UP;     // W
            case 0x1F: return KEY_DOWN;   // S
            case 0x1E: return KEY_LEFT;   // A
            case 0x20: return KEY_RIGHT;  // D
            case 0x10: case 0x01: return KEY_QUIT;  // Q / ESC
        }
    }
    return KEY_NONE;
}

// ══ Game logic ══════════════════════════════════════════════

static void init_game(void) {
    snake_len = 3;
    int cx = GRID_X0 + GRID_W / 2;
    int cy = GRID_Y0 + GRID_H / 2;
    snake[0].x = cx - 2; snake[0].y = cy;
    snake[1].x = cx - 1; snake[1].y = cy;
    snake[2].x = cx;     snake[2].y = cy;
    dir = DIR_RIGHT;
    next_dir = DIR_RIGHT;
    score = 0;
    game_over = 0;
    running = 1;
    last_move_tick = 0;
    spawn_food();
}

static void move_snake(void) {
    if (game_over) return;

    // Apply buffered direction (no 180° reversal)
    if ((dir == DIR_UP    && next_dir != DIR_DOWN)  ||
        (dir == DIR_DOWN  && next_dir != DIR_UP)    ||
        (dir == DIR_LEFT  && next_dir != DIR_RIGHT) ||
        (dir == DIR_RIGHT && next_dir != DIR_LEFT)) {
        dir = next_dir;
    }

    pos_t head = snake[snake_len - 1];
    pos_t nh;
    nh.x = head.x; nh.y = head.y;
    switch (dir) {
        case DIR_UP:    nh.y--; break;
        case DIR_DOWN:  nh.y++; break;
        case DIR_LEFT:  nh.x--; break;
        case DIR_RIGHT: nh.x++; break;
    }

    // Wall collision
    if (nh.x < GRID_X0 || nh.x >= GRID_X0 + GRID_W ||
        nh.y < GRID_Y0 || nh.y >= GRID_Y0 + GRID_H) {
        game_over = 1;
        return;
    }

    int ate = (nh.x == food_x && nh.y == food_y);

    // Self collision (skip tail if we're eating, since tail will move)
    int check = snake_len - (ate ? 0 : 1);
    for (int i = 0; i < check; i++) {
        if (snake[i].x == nh.x && snake[i].y == nh.y) {
            game_over = 1;
            return;
        }
    }

    // Advance
    if (snake_len < MAX_LEN) {
        snake[snake_len] = nh;
        snake_len++;
    }

    if (!ate) {
        // Shift array: drop tail
        for (int i = 0; i < snake_len - 1; i++)
            snake[i] = snake[i + 1];
        snake_len--;
    } else {
        score++;
        spawn_food();
    }
}

// ══ Full frame render ═══════════════════════════════════════

static void render_frame(void) {
    // We redraw the entire frame every tick for simplicity.
    // The screen is only 320x200 = 64K bytes, fine for user mode.     
    clear_screen(COL_BLACK);
    draw_status();
    draw_border();
    draw_food();
    draw_snake();
    if (game_over) {
        draw_game_over();
    }
}

// ══ Main game loop ══════════════════════════════════════════

int main(void) {
    sys_debug("[GFX] Snake starting...\r\n");
    sys_set_video_mode(1);
    sys_clear_keybuf();

    init_game();
    render_frame();

    // Game loop: poll timer and keyboard
    while (running) {
        // Read and process ALL buffered keys
        while (1) {
            int k = read_input();
            if (k == KEY_NONE) break;

            if (game_over) {
                running = 0;  // any key exits
                break;
            }

            switch (k) {
                case KEY_UP:    if (dir != DIR_DOWN)  next_dir = DIR_UP;    break;
                case KEY_DOWN:  if (dir != DIR_UP)    next_dir = DIR_DOWN;  break;
                case KEY_LEFT:  if (dir != DIR_RIGHT) next_dir = DIR_LEFT;  break;
                case KEY_RIGHT: if (dir != DIR_LEFT)  next_dir = DIR_RIGHT; break;
                case KEY_QUIT:  running = 0; break;
            }
        }

        // Check game tick: 8 timer ticks ≈ 160ms @ 50Hz
        uint32_t now = sys_get_ticks();
        if (!game_over && now - last_move_tick >= 8) {
            last_move_tick = now;
            move_snake();
            render_frame();
        }

        // Small delay to avoid busy-waiting
        for (volatile int i = 0; i < 1000; i++);
    }

    sys_debug("[GFX] Snake done, exiting\r\n");
    sys_exit(0);
    return 0;
}
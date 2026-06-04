#include "../include/shell.h"
#include "../include/vga.h"
#include "../include/keyboard.h"
#include "../include/timer.h"
#include "../include/string.h"
#include "../include/pmm.h"
#include "../include/mm.h"
#include "../include/stdio.h"
#include "../include/loader.h"
#include "../include/paging.h"
#include "../include/interrupts.h"
#include "../include/io.h"
#include "../include/scheduler.h"

#define LINE_BUF_SIZE 256

static char line_buffer[LINE_BUF_SIZE];
static size_t line_pos = 0;

static void shell_prompt(void) {
    vga_writestring("TinyOS> ");
}

static void shell_handle_command(const char* cmd);

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

// Snake game implementation (integrated into kernel)
#define SNAKE_MAX_LENGTH 100

typedef struct {
    int x;
    int y;
} pos_t;

static pos_t snake[SNAKE_MAX_LENGTH];
static int snake_len;
static volatile int dir;
static volatile int next_dir;
static int food_x, food_y;
static int score;
static volatile int game_over;
static volatile int running;
static int game_mode;
static volatile uint32_t last_move_tick;
static pos_t old_tail;

#define GAME_X0 1
#define GAME_Y0 1
#define GAME_X1 (VGA_WIDTH - 2)
#define GAME_Y1 (VGA_HEIGHT - 2)
#define GAME_WIDTH (GAME_X1 - GAME_X0 + 1)
#define GAME_HEIGHT (GAME_Y1 - GAME_Y0 + 1)

#define DIR_UP 0
#define DIR_DOWN 1
#define DIR_LEFT 2
#define DIR_RIGHT 3

static void serial_write(char c) {
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, c);
}

static void serial_string(const char* s) {
    while (*s) serial_write(*s++);
}

static void serial_hex(uint32_t n) {
    char hex[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        serial_write(hex[(n >> i) & 0xF]);
    }
}

static void put_cell(int x, int y, char c, uint8_t color) {
    static uint16_t* video_mem = (uint16_t*)0xB8000;
    video_mem[y * VGA_WIDTH + x] = (uint16_t)c | (uint16_t)color << 8;
}

static void draw_header(void) {
    for (int x = 0; x < VGA_WIDTH; x++) {
        put_cell(x, 0, ' ', VGA_COLOR_BLUE | (VGA_COLOR_WHITE << 4));
    }
    vga_set_cursor(0, 0);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLUE);
    printf("  Snake Game - Score: %d  (WASD/Arrows: move, Q/ESC: quit)", score);
}

static void draw_border(void) {
    uint8_t color = VGA_COLOR_LIGHT_BROWN;
    
    for (int x = GAME_X0; x <= GAME_X1; x++) {
        put_cell(x, GAME_Y0, '-', color);
        put_cell(x, GAME_Y1, '-', color);
    }
    
    for (int y = GAME_Y0; y <= GAME_Y1; y++) {
        put_cell(GAME_X0, y, '|', color);
        put_cell(GAME_X1, y, '|', color);
    }
    
    put_cell(GAME_X0, GAME_Y0, '+', color);
    put_cell(GAME_X1, GAME_Y0, '+', color);
    put_cell(GAME_X0, GAME_Y1, '+', color);
    put_cell(GAME_X1, GAME_Y1, '+', color);
}

static void draw_snake(void) {
    for (int i = 0; i < snake_len; i++) {
        char c = (i == 0) ? 'O' : 'o';
        put_cell(snake[i].x, snake[i].y, c, VGA_COLOR_GREEN);
    }
}

static void draw_food(void) {
    put_cell(food_x, food_y, '*', VGA_COLOR_LIGHT_RED);
}

static void draw_game_over(void) {
    const char* msg = " GAME OVER! Press any key to exit ";
    int msg_len = strlen(msg);
    int start_x = (VGA_WIDTH - msg_len) / 2;
    int start_y = VGA_HEIGHT / 2;
    
    uint8_t color = VGA_COLOR_RED | (VGA_COLOR_WHITE << 4);
    for (int x = start_x; x < start_x + msg_len; x++) {
        put_cell(x, start_y, ' ', color);
    }
    vga_set_cursor(start_x, start_y);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
    printf("%s", msg);
}

static void spawn_food(void) {
    while (1) {
        food_x = GAME_X0 + 1 + (timer_get_ticks() % (GAME_WIDTH - 2));
        food_y = GAME_Y0 + 1 + ((timer_get_ticks() * 13) % (GAME_HEIGHT - 2));
        
        int valid = 1;
        for (int i = 0; i < snake_len; i++) {
            if (snake[i].x == food_x && snake[i].y == food_y) {
                valid = 0;
                break;
            }
        }
        if (valid) break;
    }
}

static void snake_raw_cb(uint8_t scancode, uint8_t extended) {
    serial_string("[SNAKE] Key: 0x");
    serial_hex(scancode);
    serial_string("\n");
    
    if (game_over) {
        running = 0;
        return;
    }

    if (extended) {
        switch (scancode) {
            case 0x48: if (dir != DIR_DOWN)  next_dir = DIR_UP;    break;
            case 0x50: if (dir != DIR_UP)    next_dir = DIR_DOWN;  break;
            case 0x4B: if (dir != DIR_RIGHT) next_dir = DIR_LEFT;  break;
            case 0x4D: if (dir != DIR_LEFT)  next_dir = DIR_RIGHT; break;
        }
    } else {
        switch (scancode) {
            case 0x11: if (dir != DIR_DOWN)  next_dir = DIR_UP;    break;
            case 0x1F: if (dir != DIR_UP)    next_dir = DIR_DOWN;  break;
            case 0x1E: if (dir != DIR_RIGHT) next_dir = DIR_LEFT;  break;
            case 0x20: if (dir != DIR_LEFT)  next_dir = DIR_RIGHT; break;
            case 0x10: case 0x01: running = 0; break;
        }
    }
}

static volatile int needs_render;

static void snake_tick(void) {
    if (!running || game_over) return;
    
    uint32_t current_tick = timer_get_ticks();
    if (current_tick - last_move_tick < 10) {
        return;
    }
    last_move_tick = current_tick;
    
    dir = next_dir;
    
    old_tail = snake[snake_len - 1];
    for (int i = snake_len - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }
    
    switch (dir) {
        case DIR_UP:    snake[0].y--; break;
        case DIR_DOWN:  snake[0].y++; break;
        case DIR_LEFT:  snake[0].x--; break;
        case DIR_RIGHT: snake[0].x++; break;
    }
    
    if (snake[0].x <= GAME_X0 || snake[0].x >= GAME_X1 ||
        snake[0].y <= GAME_Y0 || snake[0].y >= GAME_Y1) {
        game_over = 1;
        needs_render = 1;
        return;
    }
    
    for (int i = 1; i < snake_len; i++) {
        if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
            game_over = 1;
            needs_render = 1;
            return;
        }
    }
    
    if (snake[0].x == food_x && snake[0].y == food_y) {
        if (snake_len < SNAKE_MAX_LENGTH) {
            snake[snake_len] = old_tail;
            snake_len++;
        }
        score += 10;
        spawn_food();
    }
    
    needs_render = 1;  // Signal main loop to render
}

static void render_all(void) {
    vga_clear_screen(VGA_COLOR_BLACK);
    draw_header();
    draw_border();
    draw_food();
    draw_snake();
    if (game_over) draw_game_over();
}

static void render_update(void) {
    if (!game_over) {
        draw_header();
        // Only clear the old tail cell, not the whole screen (preserves border)
        put_cell(old_tail.x, old_tail.y, ' ', VGA_COLOR_BLACK);
        draw_border();
        draw_snake();
        draw_food();
    } else {
        draw_game_over();
    }
}

static void snake_start(int mode) {
    game_mode = mode;
    score = 0;
    dir = DIR_RIGHT;
    next_dir = DIR_RIGHT;
    game_over = 0;
    running = 1;
    snake_len = 3;
    
    snake[0].x = GAME_X0 + 10;
    snake[0].y = GAME_Y0 + 10;
    snake[1].x = GAME_X0 + 9;
    snake[1].y = GAME_Y0 + 10;
    snake[2].x = GAME_X0 + 8;
    snake[2].y = GAME_Y0 + 10;
    
    last_move_tick = timer_get_ticks();
    
    spawn_food();
    
    // snake_start is called from within the keyboard IRQ1 handler
    // (shell_char_callback -> shell_handle_command -> snake_start).
    // We must send EOI for IRQ1 so the PIC allows new keyboard interrupts.
    outb(0x20, 0x20);  // Send EOI to master PIC for IRQ1

    keyboard_register_raw_callback(snake_raw_cb);
    keyboard_register_char_callback(NULL);
    timer_register_tick_callback(snake_tick);
    
    render_all();

    // Enable interrupts right before the game loop.
    // IF is still 0 from the IRQ handler entry, so no interrupts
    // can fire during setup above.
    enable_interrupts();

    while (running) {
        asm volatile("hlt");
        if (needs_render) {
            needs_render = 0;
            render_update();
        }
    }
    
    timer_register_tick_callback(NULL);
    keyboard_register_raw_callback(NULL);
    keyboard_register_char_callback(shell_char_callback);
    
    vga_clear_screen(VGA_COLOR_BLACK);
    vga_set_cursor(0, 0);
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    printf("Game Over! Final Score: %d\n", score);
    shell_prompt();
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

// Deadline tick (absolute timer tick count) for schedtest tasks.
// When timer_get_ticks() >= this value, tasks call task_exit().
static volatile uint32_t schedtest_deadline = 0;

static void schedtest_a(void) {
    while (timer_get_ticks() < schedtest_deadline) {
        printf("A ");
        asm volatile("hlt");
    }
    printf("\n[task_a] Time's up, exiting.\n");
    task_exit();
}

static void schedtest_b(void) {
    while (timer_get_ticks() < schedtest_deadline) {
        printf("B ");
        asm volatile("hlt");
    }
    printf("\n[task_b] Time's up, exiting.\n");
    task_exit();
}

static void cmd_schedtest(const char* args) {
    // Parse optional duration in seconds (default 10s)
    uint32_t duration = 10;
    if (args && *args >= '0' && *args <= '9') {
        duration = 0;
        while (*args >= '0' && *args <= '9') {
            duration = duration * 10 + (*args++ - '0');
        }
        if (duration == 0) duration = 1;
        if (duration > 300) duration = 300;  // cap at 5 minutes
    }
    // Timer runs at 50 Hz
    schedtest_deadline = timer_get_ticks() + duration * 50;
    printf("Starting scheduler test (A/B tasks) for %u seconds...\n", duration);
    task_create("task_a", schedtest_a);
    task_create("task_b", schedtest_b);
    printf("Tasks created. They will auto-stop after %u seconds.\n", duration);
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
            printf("  pageinfo   - Show page table info\n");
            printf("  snake      - Play Snake game (text mode)\n");
            printf("  gfxsnake   - Play Snake game (pixel graphics mode)\n");
            printf("  hello      - Run hello user program\n");
            printf("  schedtest [N]- Start scheduler test for N seconds (default 10)\n");
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
    } else if (strcmp(cmd, "pageinfo") == 0) {
        paging_dump_info();
    } else if (strcmp(cmd, "snake") == 0) {
        snake_start(0);
    } else if (strcmp(cmd, "gfxsnake") == 0) {
        run_loaded_user();
    } else if (strcmp(cmd, "gtest") == 0) {
        vga_gfx_test();
    } else if (strcmp(cmd, "runuser") == 0) {
        run_loaded_user();
    } else if (strcmp(cmd, "testuser") == 0) {
        extern void test_user_mode(void);
        test_user_mode();
    } else if (strcmp(cmd, "hello") == 0) {
        run_hello_user();
    } else if (strncmp(cmd, "schedtest", 9) == 0) {
        const char* args = cmd + 9;
        while (*args == ' ') args++;
        cmd_schedtest(*args ? args : NULL);
    } else {
        printf("Unknown command: %s\n", cmd);
        printf("Type 'help' for available commands.\n");
    }
}

void shell_init(void) {
    keyboard_register_char_callback(shell_char_callback);
    shell_prompt();
}

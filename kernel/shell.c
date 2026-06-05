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
#include "../include/ata.h"
#include "../include/fat16.h"
#include "../include/pci.h"
#include "../include/ne2000.h"
#include "../include/net.h"
#include "../include/serial.h"
#include "../include/framebuf.h"
#include "../include/window.h"
#include "../include/mouse.h"
#include "../include/vbe.h"
#include "../include/builtin_font.h"
#include "../include/webserver.h"
#include "../include/rtc.h"
#include "../include/prng.h"
#include "../include/ipc.h"

#define LINE_BUF_SIZE 256

static char line_buffer[LINE_BUF_SIZE];
static size_t line_pos = 0;

static void shell_prompt(void) {
    vga_writestring("TinyOS> ");
    serial_writestring("TinyOS> ");
}

static void shell_handle_command(const char* cmd);

/* UDP recv callback for the 'recv' command — prints received data to screen */
static void shell_udp_recv_cb(uint32_t src_ip, uint16_t src_port,
                                uint16_t dst_port,
                                const uint8_t* data, uint16_t len) {
    printf("\n[UDP %u.%u.%u.%u:%u -> :%u] (%u bytes)\n",
           src_ip & 0xFF, (src_ip >> 8) & 0xFF,
           (src_ip >> 16) & 0xFF, (src_ip >> 24) & 0xFF,
           src_port, dst_port, len);
    /* Print data as text (up to 256 chars) */
    uint16_t show = len < 256 ? len : 256;
    for (uint16_t i = 0; i < show; i++) {
        char c = (char)data[i];
        if (c >= 32 && c < 127) {
            vga_putchar(c);
            serial_putchar(c);
        }
    }
    if (show < len) printf("...");
    printf("\n");
}

/* TCP recv callback for 'tcp-recv' command */
static void shell_tcp_recv_cb(uint32_t src_ip, uint16_t src_port,
                               const uint8_t* data, uint16_t len) {
    printf("\n[TCP %u.%u.%u.%u:%u] (%u bytes)\n",
           src_ip & 0xFF, (src_ip >> 8) & 0xFF,
           (src_ip >> 16) & 0xFF, (src_ip >> 24) & 0xFF,
           src_port, len);
    uint16_t show = len < 512 ? len : 512;
    for (uint16_t i = 0; i < show; i++) {
        char c = (char)data[i];
        if (c >= 32 && c < 127) {
            vga_putchar(c);
            serial_putchar(c);
        }
    }
    if (show < len) printf("...");
    printf("\n");
}

void shell_char_callback(char c) {
    if (c == '\n' || c == '\r') {
        vga_putchar('\n');
        serial_putchar('\n');
        line_buffer[line_pos] = '\0';
        shell_handle_command(line_buffer);
        line_pos = 0;
        shell_prompt();
    } else if (c == '\b' || c == 127) {
        if (line_pos > 0) {
            line_pos--;
            vga_putchar('\b');
            serial_putchar('\b');
            serial_putchar(' ');
            serial_putchar('\b');
        }
    } else if (c >= 32 && c < 127) {
        if (line_pos < LINE_BUF_SIZE - 1) {
            line_buffer[line_pos++] = c;
            vga_putchar(c);
            serial_putchar(c);
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

/* Debug hex output to serial (used in interrupt context) */
static void serial_hex(uint32_t n) {
    char hex[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        serial_putchar(hex[(n >> i) & 0xF]);
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
    serial_writestring("[SNAKE] Key: 0x");
    serial_hex(scancode);
    serial_putchar('\n');
    
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

/* Parse "A.B.C.D" into 4 octets. Returns 1 on success, 0 on failure. */
static int parse_ip(const char* s, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d) {
    *a = *b = *c = *d = 0;
    int octets = 0;
    uint32_t* targets[4] = {a, b, c, d};
    while (*s && octets < 4) {
        if (*s >= '0' && *s <= '9') {
            *targets[octets] = *targets[octets] * 10 + (*s - '0');
        } else if (*s == '.') {
            octets++;
            if (octets >= 4) return 0;
        } else {
            break;
        }
        s++;
    }
    /* Validate all octets are 0-255 */
    for (int i = 0; i <= octets; i++) {
        if (*targets[i] > 255) return 0;
    }
    return (octets == 3) ? 1 : 0;
}

// Deadline tick (absolute timer tick count) for schedtest tasks.
// When timer_get_ticks() >= this value, tasks call task_exit().
static volatile uint32_t schedtest_deadline = 0;

static void schedtest_a(void) {
    while (timer_get_ticks() < schedtest_deadline) {
        printf("A ");
        task_sleep(200);  /* sleep 200ms, exercises yield/sleep */
    }
    printf("\n[task_a] Time's up, exiting.\n");
    task_exit();
}

static void schedtest_b(void) {
    while (timer_get_ticks() < schedtest_deadline) {
        printf("B ");
        task_sleep(200);  /* sleep 200ms, exercises yield/sleep */
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

// ── IPC Test command ─────────────────────────────────────────────────
// Three-phase test: Pipe → Message Queue → Shared Memory

static volatile int ipc_phase_done = 0;

static int ipc_pipe_id = -1;
static volatile int ipc_pipe_result = -1;

static void ipc_pipe_producer(void) {
    const char* msg = "Hello IPC!";
    int len = 0;
    while (msg[len]) len++;
    pipe_write(ipc_pipe_id, msg, len);
    task_exit();
}

static void ipc_pipe_consumer(void) {
    char buf[64];
    int n = pipe_read(ipc_pipe_id, buf, sizeof(buf));
    if (n == 10 && memcmp(buf, "Hello IPC!", 10) == 0) {
        ipc_pipe_result = 0;
    } else {
        ipc_pipe_result = 1;
    }
    ipc_phase_done = 1;
    task_exit();
}

static int ipc_mq_id = -1;
static volatile int ipc_mq_result = -1;

static void ipc_mq_sender(void) {
    mq_send(ipc_mq_id, "msg_one", 7);
    mq_send(ipc_mq_id, "msg_two", 7);
    mq_send(ipc_mq_id, "msg_thr", 7);
    task_exit();
}

static void ipc_mq_receiver(void) {
    char buf[64];
    int ok = 1;
    int n;

    n = mq_recv(ipc_mq_id, buf, sizeof(buf));
    if (n != 7 || memcmp(buf, "msg_one", 7) != 0) ok = 0;

    n = mq_recv(ipc_mq_id, buf, sizeof(buf));
    if (n != 7 || memcmp(buf, "msg_two", 7) != 0) ok = 0;

    n = mq_recv(ipc_mq_id, buf, sizeof(buf));
    if (n != 7 || memcmp(buf, "msg_thr", 7) != 0) ok = 0;

    ipc_mq_result = ok ? 0 : 1;
    ipc_phase_done = 1;
    task_exit();
}

static volatile int ipc_shm_result = -1;

static void ipc_shm_writer(void) {
    uint32_t* p = (uint32_t*)shm_create("test", 64);
    if (p) {
        p[0] = 0xDEADBEEF;
        p[1] = 42;
    }
    task_exit();
}

static void ipc_shm_reader(void) {
    task_sleep(100);
    uint32_t* p = (uint32_t*)shm_open("test");
    if (p && p[0] == 0xDEADBEEF && p[1] == 42) {
        ipc_shm_result = 0;
    } else {
        ipc_shm_result = 1;
    }
    shm_close("test");
    ipc_phase_done = 1;
    task_exit();
}

static void cmd_ipctest(void) {
    printf("=== IPC Test ===\n");

    /* Phase 1: Pipe */
    printf("[Pipe] Creating pipe...\n");
    ipc_pipe_id = pipe_create();
    if (ipc_pipe_id < 0) {
        printf("[Pipe] FAILED: pipe_create returned -1\n");
        return;
    }
    ipc_phase_done = 0;
    ipc_pipe_result = -1;
    task_create("ipc_cons", ipc_pipe_consumer);
    task_create("ipc_prod", ipc_pipe_producer);
    while (!ipc_phase_done) { task_sleep(50); }
    pipe_close(ipc_pipe_id);
    printf("[Pipe] %s\n", ipc_pipe_result == 0 ? "PASS" : "FAIL");

    /* Phase 2: Message Queue */
    printf("[MQ] Creating message queue...\n");
    ipc_mq_id = mq_create();
    if (ipc_mq_id < 0) {
        printf("[MQ] FAILED: mq_create returned -1\n");
        return;
    }
    ipc_phase_done = 0;
    ipc_mq_result = -1;
    task_create("ipc_mqrx", ipc_mq_receiver);
    task_create("ipc_mqtx", ipc_mq_sender);
    while (!ipc_phase_done) { task_sleep(50); }
    mq_close(ipc_mq_id);
    printf("[MQ] %s\n", ipc_mq_result == 0 ? "PASS" : "FAIL");

    /* Phase 3: Shared Memory */
    printf("[SHM] Creating shared memory 'test'...\n");
    ipc_phase_done = 0;
    ipc_shm_result = -1;
    task_create("ipc_shmr", ipc_shm_reader);
    task_create("ipc_shmw", ipc_shm_writer);
    while (!ipc_phase_done) { task_sleep(50); }
    printf("[SHM] %s\n", ipc_shm_result == 0 ? "PASS" : "FAIL");

    printf("=== IPC Test Done ===\n");
}

// ── GUI command ──────────────────────────────────────────────────────
// Start VBE graphics mode GUI with window manager.
// Press Escape to exit back to shell.

extern void vga_set_mode03h(void);
extern void mouse_handler(void);

static volatile int gui_running = 0;

static void gui_raw_cb(uint8_t scancode, uint8_t extended) {
    (void)extended;
    if (scancode == 0x01) {  // Escape key press
        gui_running = 0;
    }
}

// Demo window draw callback for system info
static void wm_demo_info_draw(window_t* win) {
    const uint8_t* font = builtin_font_get();

    uint32_t fg = RGB(0x00, 0x00, 0x00);
    uint32_t bg = WM_COLOR_CLIENT_BG;
    int x = win->x + 8;
    int y = win->y + 8;
    int lh = 18;

    uint32_t secs = timer_get_ticks() / 50;
    uint32_t heap_used = kmalloc_get_used();
    uint32_t heap_total = kmalloc_get_total();
    uint32_t mem_free_mb = pmm_get_free_pages() * 4 / 1024;

    char buf[64];
    fb_drawstring(x, y, "TinyOS v0.1 - System Information", RGB(0x00, 0x00, 0x80), bg, font);
    y += lh * 2;

    sprintf(buf, "Uptime: %u s", secs);
    fb_drawstring(x, y, buf, fg, bg, font); y += lh;

    sprintf(buf, "Heap: %u KB / %u KB", heap_used / 1024, heap_total / 1024);
    fb_drawstring(x, y, buf, fg, bg, font); y += lh;

    sprintf(buf, "Free memory: %u MB", mem_free_mb);
    fb_drawstring(x, y, buf, fg, bg, font); y += lh;

    sprintf(buf, "Resolution: %dx%d %dbpp", fb.width, fb.height, fb.bpp);
    fb_drawstring(x, y, buf, fg, bg, font); y += lh;

    fb_drawstring(x, y, "---", fg, bg, font); y += lh;

    fb_drawstring(x, y, "Click & drag title bar to move window", RGB(0x80, 0x80, 0x80), bg, font); y += lh;
    fb_drawstring(x, y, "Click X to close", RGB(0x80, 0x80, 0x80), bg, font);
}

// Mouse event wrapper — forwards mouse events to WM
static void gui_mouse_cb(int x, int y, uint8_t buttons) {
    wm_handle_mouse(x, y, buttons);
}

static void cmd_gui(void) {
    // cmd_gui is called from the keyboard IRQ1 handler (shell_char_callback → shell_handle_command).
    // Send EOI for IRQ1 so PIC allows new keyboard interrupts during GUI operation.
    outb(0x20, 0x20);

    /* Register keyboard raw callback (Escape to exit) and disable shell input */
    gui_running = 1;
    keyboard_register_raw_callback(gui_raw_cb);
    keyboard_register_char_callback(NULL);

    /* Initialize framebuffer (VBE graphics mode 800x600x32) */
    if (fb_init(800, 600, 32) != 0) {
        printf("Failed to initialize framebuffer (no VBE support?)\n");
        keyboard_register_raw_callback(NULL);
        keyboard_register_char_callback(shell_char_callback);
        gui_running = 0;
        return;
    }

    /* Init PS/2 mouse (IRQ 12) */
    mouse_init();
    mouse_register_callback(gui_mouse_cb);
    register_interrupt_handler(44, mouse_handler);
    pic_unmask_irq(12);

    /* Init window manager */
    wm_init();

    /* Create system info demo window */
    window_t* win = wm_create_window(50, 50, 500, 280,
        "TinyOS System Info", wm_demo_info_draw, NULL);
    if (win) {
        printf("[OK] Demo window created\n");
    }

    /* Re-enable interrupts before entering the rendering loop */
    enable_interrupts();

    /* Initial draw */
    wm_redraw();
    fb_flip();
    wm_seed_cursor();

    /* GUI rendering loop */
    while (gui_running) {
        if (wm_redraw_needed()) {
            wm_redraw();
            fb_flip();
            wm_seed_cursor();
        } else if (wm_cursor_moved()) {
            wm_update_cursor();
        }
        asm volatile("hlt");
    }

    /* ── Cleanup and return to text mode ── */
    disable_interrupts();

    keyboard_register_raw_callback(NULL);
    keyboard_register_char_callback(shell_char_callback);

    /* Disable mouse IRQ and unregister callback */
    pic_mask_irq(12);
    mouse_register_callback(NULL);

    /* Restore VGA text mode — reset everything to clean state */
    vbe_disable();
    vga_set_mode03h();
    vga_initialize();
    printf("GUI exited.\n");

    enable_interrupts();
    shell_prompt();
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
            printf("  schedtest [N]- Start scheduler test for N seconds\n");
            printf("  ipctest      - Run IPC test (Pipe, MQ, SharedMem)\n");
            printf("  gui        - Start graphical UI (VBE mode)\n");
            printf("  ls         - List files on disk\n");
            printf("  cat <file> - Print file contents\n");
            printf("  diskinfo   - Show disk/filesystem info\n");
            printf("  pci        - List PCI devices\n");
            printf("  net        - Show network config\n");
            printf("  ping <ip>  - Send ICMP echo request\n");
            printf("  send <ip> <port> <msg> - Send UDP packet\n");
            printf("  recv <port> - Listen for UDP packets (5s)\n");
            printf("  tcp-recv <port> - Listen for TCP connections (10s)\n");
            printf("  dhcp       - Request IP via DHCP\n");
            printf("  arp        - Show ARP cache\n");
            printf("  arp -c     - Clear ARP cache\n");
            printf("  netstat    - Show network statistics\n");
            printf("  netstat -r - Reset network statistics\n");
            printf("  rand       - Show a random number (PRNG)\n");
            printf("  write <file> <text> - Write text to file (FAT16)\n");
            printf("  rm <file>  - Delete a file from disk\n");
            printf("  webserver  - Start HTTP server (port 80, hostfwd :8088)\n");
            printf("  webserver stop - Stop HTTP server\n");
            printf("  date       - Show current date/time (CMOS RTC)\n");
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
    } else if (strcmp(cmd, "ipctest") == 0) {
        cmd_ipctest();
    } else if (strcmp(cmd, "gui") == 0) {
        cmd_gui();
    } else if (strcmp(cmd, "ls") == 0) {
        fat16_list();
    } else if (strncmp(cmd, "cat ", 4) == 0) {
        const char* fname = cmd + 4;
        while (*fname == ' ') fname++;
        fat16_entry_t entry;
        if (fat16_find(fname, &entry) == 0) {
            /* Read file into a buffer and print */
            static char cat_buf[4096];
            uint32_t to_read = entry.file_size;
            if (to_read > sizeof(cat_buf) - 1) to_read = sizeof(cat_buf) - 1;
            uint32_t got = fat16_read(&entry, 0, cat_buf, to_read);
            cat_buf[got] = '\0';
            printf("%s", cat_buf);
            if (got > 0 && cat_buf[got-1] != '\n') printf("\n");
            printf("(%u bytes)\n", entry.file_size);
        } else {
            printf("File not found: %s\n", fname);
        }
    } else if (strcmp(cmd, "diskinfo") == 0) {
        const fat16_bpb_t* b = fat16_get_bpb();
        if (!b) {
            printf("No filesystem mounted\n");
        } else {
            printf("Disk info:\n");
            printf("  Total size:      %u KB\n", b->total_size / 1024);
            printf("  Bytes/sector:    %u\n", b->bytes_per_sector);
            printf("  Sectors/cluster: %u\n", b->sectors_per_cluster);
            printf("  Total clusters:  %u\n", b->total_clusters);
            printf("  Root entries:    %u\n", b->root_entry_count);
        }
    } else if (strcmp(cmd, "pci") == 0) {
        pci_list_devices();
    } else if (strcmp(cmd, "net") == 0) {
        uint32_t ip, gw, mask;
        net_get_config(&ip, &gw, &mask);
        if (ip == 0) {
            printf("Network not initialized\n");
        } else {
            printf("Network config:\n");
            printf("  IP:      %u.%u.%u.%u\n", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
            printf("  Gateway: %u.%u.%u.%u\n", gw & 0xFF, (gw >> 8) & 0xFF, (gw >> 16) & 0xFF, (gw >> 24) & 0xFF);
            printf("  Mask:    %u.%u.%u.%u\n", mask & 0xFF, (mask >> 8) & 0xFF, (mask >> 16) & 0xFF, (mask >> 24) & 0xFF);
            const uint8_t* mac = ne2000_get_mac();
            printf("  MAC:     %02x:%02x:%02x:%02x:%02x:%02x\n",
                   mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
    } else if (strncmp(cmd, "ping ", 5) == 0) {
        const char* ipstr = cmd + 5;
        while (*ipstr == ' ') ipstr++;
        uint32_t a, b, c, d;
        if (parse_ip(ipstr, &a, &b, &c, &d)) {
            uint32_t target_ip = IP4(a, b, c, d);
            printf("Pinging %u.%u.%u.%u...\n", a, b, c, d);

            int ret = net_send_icmp_echo(target_ip, 1, 1);
            if (ret < 0) {
                /* ARP not cached, poll for ARP reply */
                uint32_t start = timer_get_ticks();
                int resolved = 0;
                while (timer_get_ticks() - start < 10) {  /* ~200ms at 50Hz */
                    ne2000_poll_recv();
                    ret = net_send_icmp_echo(target_ip, 1, 1);
                    if (ret >= 0) { resolved = 1; break; }
                }
                if (resolved) {
                    printf("Ping sent to %u.%u.%u.%u\n", a, b, c, d);
                } else {
                    printf("ARP timeout: could not resolve %u.%u.%u.%u\n", a, b, c, d);
                }
            } else {
                printf("Ping sent to %u.%u.%u.%u\n", a, b, c, d);
            }
        } else {
            printf("Usage: ping A.B.C.D\n");
        }
    } else if (strncmp(cmd, "send ", 5) == 0) {
        /* send <ip> <port> <msg> */
        const char* arg = cmd + 5;
        while (*arg == ' ') arg++;
        uint32_t a, b, c, d;
        if (parse_ip(arg, &a, &b, &c, &d)) {
            /* Skip past IP */
            while (*arg && *arg != ' ') arg++;
            while (*arg == ' ') arg++;
            /* Parse port */
            uint32_t port = 0;
            while (*arg >= '0' && *arg <= '9') {
                port = port * 10 + (*arg++ - '0');
            }
            while (*arg == ' ') arg++;
            /* Rest is message */
            uint32_t target_ip = IP4(a, b, c, d);

            int ret = net_send_udp(target_ip, (uint16_t)port, 1234, arg, strlen(arg));
            if (ret < 0) {
                uint32_t start = timer_get_ticks();
                int resolved = 0;
                while (timer_get_ticks() - start < 10) {
                    ne2000_poll_recv();
                    ret = net_send_udp(target_ip, (uint16_t)port, 1234, arg, strlen(arg));
                    if (ret >= 0) { resolved = 1; break; }
                }
                if (resolved) {
                    printf("Sent %u bytes to %u.%u.%u.%u:%u\n", strlen(arg), a, b, c, d, port);
                } else {
                    printf("ARP timeout: could not resolve %u.%u.%u.%u\n", a, b, c, d);
                }
            } else {
                printf("Sent %u bytes to %u.%u.%u.%u:%u\n", strlen(arg), a, b, c, d, port);
            }
        } else {
            printf("Usage: send A.B.C.D <port> <message>\n");
        }
    } else if (strncmp(cmd, "webserver", 9) == 0) {
        const char* arg = cmd + 9;
        while (*arg == ' ') arg++;
        if (strcmp(arg, "stop") == 0) {
            webserver_stop();
        } else {
            webserver_start();
        }
    } else if (strcmp(cmd, "date") == 0) {
        rtc_time_t tm;
        rtc_read_time(&tm);
        printf("%04u-%02u-%02u %02u:%02u:%02u\n",
               tm.year, tm.month, tm.day,
               tm.hour, tm.minute, tm.second);
    } else if (strncmp(cmd, "arp", 3) == 0) {
        const char* arg = cmd + 3;
        while (*arg == ' ') arg++;
        if (strcmp(arg, "-c") == 0 || strcmp(arg, "clear") == 0) {
            net_arp_clear();
            printf("ARP cache cleared.\n");
        } else {
            printf("ARP cache:\n");
            int found = 0;
            for (int i = 0; i < ARP_TABLE_SIZE; i++) {
                const arp_entry_t* e = net_arp_table_get(i);
                if (e) {
                    printf("  %u.%u.%u.%u -> %02x:%02x:%02x:%02x:%02x:%02x\n",
                           e->ip & 0xFF, (e->ip >> 8) & 0xFF,
                           (e->ip >> 16) & 0xFF, (e->ip >> 24) & 0xFF,
                           e->mac[0], e->mac[1], e->mac[2],
                           e->mac[3], e->mac[4], e->mac[5]);
                    found++;
                }
            }
            if (found == 0) printf("  (empty)\n");
        }
    } else if (strcmp(cmd, "netstat") == 0) {
        const net_stats_t* s = net_get_stats();
        printf("Network statistics:\n");
        printf("  RX packets: %u\n", s->rx_packets);
        printf("  TX packets: %u\n", s->tx_packets);
        printf("  RX errors:  %u\n", s->rx_errors);
        printf("  TX errors:  %u\n", s->tx_errors);
        printf("  ARP:  %u requests sent, %u replies recv\n", s->arp_requests_sent, s->arp_replies_recv);
        printf("  ICMP: %u sent, %u recv\n", s->icmp_sent, s->icmp_recv);
        printf("  UDP:  %u sent, %u recv\n", s->udp_sent, s->udp_recv);
        printf("  TCP:  %u sent, %u recv\n", s->tcp_sent, s->tcp_recv);
    } else if (strcmp(cmd, "netstat -r") == 0) {
        net_stats_reset();
        printf("Network statistics reset.\n");
    } else if (strncmp(cmd, "recv ", 5) == 0) {
        const char* arg = cmd + 5;
        while (*arg == ' ') arg++;
        uint32_t port = 0;
        while (*arg >= '0' && *arg <= '9') {
            port = port * 10 + (*arg++ - '0');
        }
        if (port == 0 || port > 65535) {
            printf("Usage: recv <port>\n");
        } else {
            printf("Listening on UDP port %u (5 seconds)...\n", port);
            /* Send EOI for keyboard IRQ1 since we're in the handler */
            outb(0x20, 0x20);
            /* Temporary recv callback that prints to screen */
            net_set_udp_callback(shell_udp_recv_cb);
            enable_interrupts();
            uint32_t deadline = timer_get_ticks() + 250;  /* 5 seconds */
            while (timer_get_ticks() < deadline) {
                asm volatile("hlt");
            }
            net_set_udp_callback(NULL);
            printf("\nDone listening on port %u.\n", port);
            shell_prompt();
        }
    } else if (strcmp(cmd, "rand") == 0) {
        prng_seed(timer_get_ticks());
        printf("%u\n", prng_next());
    } else if (strncmp(cmd, "write ", 6) == 0) {
        /* write <file> <text> */
        const char* arg = cmd + 6;
        while (*arg == ' ') arg++;
        const char* fname = arg;
        /* find end of filename */
        while (*arg && *arg != ' ') arg++;
        uint32_t fname_len = (uint32_t)(arg - fname);
        if (fname_len == 0 || fname_len > 12) {
            printf("Usage: write <file> <text>\n");
            return;
        }
        char fname_buf[13];
        memcpy(fname_buf, fname, fname_len);
        fname_buf[fname_len] = '\0';
        /* skip to text */
        while (*arg == ' ') arg++;
        if (*arg == '\0') {
            printf("Usage: write <file> <text>\n");
            return;
        }
        uint32_t text_len = strlen(arg);
        if (fat16_write(fname_buf, arg, text_len) == 0) {
            printf("Wrote %u bytes to %s\n", text_len, fname_buf);
        } else {
            printf("Failed to write %s\n", fname_buf);
        }
    } else if (strncmp(cmd, "tcp-recv ", 9) == 0) {
        const char* arg = cmd + 9;
        while (*arg == ' ') arg++;
        uint32_t port = 0;
        while (*arg >= '0' && *arg <= '9') {
            port = port * 10 + (*arg++ - '0');
        }
        if (port == 0 || port > 65535) {
            printf("Usage: tcp-recv <port>\n");
        } else {
            printf("Listening for TCP on port %u (10 seconds)...\n", port);
            outb(0x20, 0x20);  /* EOI for keyboard IRQ1 */
            net_tcp_listen((uint16_t)port);
            net_set_tcp_callback(shell_tcp_recv_cb);
            enable_interrupts();
            uint32_t deadline = timer_get_ticks() + 500;  /* 10 seconds */
            while (timer_get_ticks() < deadline) {
                asm volatile("hlt");
            }
            net_set_tcp_callback(NULL);
            printf("\nDone listening on TCP port %u.\n", port);
            shell_prompt();
        }
    } else if (strcmp(cmd, "dhcp") == 0) {
        printf("Sending DHCP Discover...\n");
        outb(0x20, 0x20);  /* EOI for keyboard IRQ1 */
        enable_interrupts();
        int ret = net_dhcp_discover();
        if (ret == 0) {
            /* Poll for DHCP Offer and wait for ACK */
            uint32_t start = timer_get_ticks();
            uint32_t ip, gw, mask;
            while (timer_get_ticks() - start < 250) {  /* 5 second timeout */
                ne2000_poll_recv();
                net_get_config(&ip, &gw, &mask);
                if (ip != 0 && ip != 0x0F00000A) break;  /* changed from default */
            }
            net_get_config(&ip, &gw, &mask);
            printf("DHCP result:\n");
            printf("  IP:      %u.%u.%u.%u\n", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);
            printf("  Gateway: %u.%u.%u.%u\n", gw & 0xFF, (gw >> 8) & 0xFF, (gw >> 16) & 0xFF, (gw >> 24) & 0xFF);
            printf("  Mask:    %u.%u.%u.%u\n", mask & 0xFF, (mask >> 8) & 0xFF, (mask >> 16) & 0xFF, (mask >> 24) & 0xFF);
        } else {
            printf("DHCP failed.\n");
        }
    } else if (strncmp(cmd, "rm ", 3) == 0) {
        const char* fname = cmd + 3;
        while (*fname == ' ') fname++;
        if (*fname == '\0') {
            printf("Usage: rm <file>\n");
        } else if (fat16_delete(fname) == 0) {
            printf("Deleted %s\n", fname);
        } else {
            printf("File not found: %s\n", fname);
        }
    } else {
        printf("Unknown command: %s\n", cmd);
        printf("Type 'help' for available commands.\n");
    }
}

void shell_init(void) {
    keyboard_register_char_callback(shell_char_callback);
    serial_register_callback(shell_char_callback);
    shell_prompt();
}

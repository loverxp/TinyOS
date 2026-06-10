/* dino.c - Chrome Dinosaur terminal game for TinyOS
 * A simple reflex game where you jump over obstacles.
 * 
 * Game mechanics:
 *   - Dino (@) on the left, obstacles (#) scroll from right
 *   - Press SPACE/W or UP to jump
 *   - Each obstacle cleared = 1 point
 *   - Collision = game over
 *   - Uses non-blocking read_key() for real-time gameplay
 */

#include <stdio.h>
#include <syscall.h>
#include <unistd.h>

/* ── Scancode constants (PS/2 set 1) ──────────────────────────── */
#define SC_SPACE   0x39
#define SC_W       0x11
#define SC_Q       0x10
#define SC_R       0x13
#define SC_UP_RAW  0x48   /* extended: key & 0x80 will be set */

/* ── Game constants ────────────────────────────────────────────── */
#define GROUND_Y    18          /* y-position of the ground row */
#define DINO_X      8           /* x-position of the dino */
#define MAX_OBS     8           /* max simultaneous obstacles */
#define SCREEN_W    78          /* width of playfield */
#define JUMP_VEL   -4          /* initial upward jump velocity */
#define GRAVITY     1           /* gravity per frame */
#define GAME_TICK   80          /* ms per game tick (sleep) */
#define OBSTACLE_SPAWN_RATE  5  /* spawn an obstacle every N frames */

/* ── Game state ────────────────────────────────────────────────── */
static int dino_y;             /* current y position of dino */
static int dino_vy;            /* vertical velocity (jump) */
static int jumping;            /* 1 if in jump arc */
static int score;              /* obstacles cleared */
static int game_over;          /* 1 if collision occurred */
static int obstacles[MAX_OBS]; /* x-positions of obstacles (0 = inactive) */
static int frame;              /* frame counter */

/* ── Reset game state to initial values ───────────────────────── */
static void reset(void) {
    dino_y = GROUND_Y;
    dino_vy = 0;
    jumping = 0;
    score = 0;
    game_over = 0;
    frame = 0;
    for (int i = 0; i < MAX_OBS; i++)
        obstacles[i] = 0;
}

/* ── Advance game state by one frame ──────────────────────────── */
static void advance(void) {
    int i;

    if (game_over) return;
    frame++;

    /* Move all obstacles one step to the left */
    for (i = 0; i < MAX_OBS; i++) {
        if (obstacles[i] > 0)
            obstacles[i]--;
    }

    /* Remove obstacles that scrolled off the left edge */
    for (i = 0; i < MAX_OBS; i++) {
        if (obstacles[i] < 0)
            obstacles[i] = 0;
    }

    /* Spawn a new obstacle periodically */
    if (frame % OBSTACLE_SPAWN_RATE == 0) {
        for (i = 0; i < MAX_OBS; i++) {
            if (obstacles[i] == 0) {
                obstacles[i] = SCREEN_W;
                break;
            }
        }
    }

    /* Jump physics: parabolic arc */
    if (jumping || dino_y < GROUND_Y) {
        dino_y += dino_vy;
        dino_vy += GRAVITY;
        if (dino_y >= GROUND_Y) {
            dino_y = GROUND_Y;
            jumping = 0;
            dino_vy = 0;
        }
    }

    /* Collision detection: dino hits a cactus */
    for (i = 0; i < MAX_OBS; i++) {
        int ox = obstacles[i];
        if (ox == 0) continue;
        /* Cactus occupies x..x+1, hits dino if dino is near ground */
        if (ox >= DINO_X - 1 && ox <= DINO_X + 1 && dino_y >= GROUND_Y - 1) {
            game_over = 1;
            return;
        }
    }

    /* Score: count obstacles that just passed the dino */
    for (i = 0; i < MAX_OBS; i++) {
        if (obstacles[i] == DINO_X - 1)
            score++;
    }
}

/* ── Render the full game screen ──────────────────────────────── */
static void draw(void) {
    int x, y, i;

    clear_screen();

    /* ── Header ── */
    printf("=== TinyOS Dino ===  Score: %d\n", score);
    if (game_over)
        printf("  *** GAME OVER! Press R=Restart  Q=Quit ***\n");
    else
        printf("  SPACE/W=Jump  Q=Quit\n");
    putchar('\n');

    /* ── Top border ── */
    putchar('+');
    for (x = 0; x < SCREEN_W; x++) putchar('-');
    putchar('+');
    putchar('\n');

    /* ── Game area ── */
    for (y = 0; y <= GROUND_Y; y++) {
        putchar('|');
        for (x = 0; x < SCREEN_W; x++) {
            char c = ' ';

            /* Draw dino: @ character */
            if (x >= DINO_X && x <= DINO_X + 1 && y == dino_y) {
                c = '@';
            }

            /* Draw obstacles: # cactus at ground and one above */
            for (i = 0; i < MAX_OBS; i++) {
                int ox = obstacles[i];
                if (ox == 0) continue;
                if ((x == ox || x == ox + 1) && (y == GROUND_Y || y == GROUND_Y - 1)) {
                    c = '#';
                }
            }

            /* Draw ground as _ characters */
            if (y == GROUND_Y && c == ' ')
                c = '_';

            putchar(c);
        }
        putchar('|');
        putchar('\n');
    }

    /* ── Bottom border ── */
    putchar('+');
    for (x = 0; x < SCREEN_W; x++) putchar('-');
    putchar('+');
    putchar('\n');
}

/* ── Process a single keypress (scancode) ─────────────────────── */
static void handle_key(uint32_t key) {
    uint8_t sc = key & 0x7F;
    uint8_t ext = (key & 0x80) ? 1 : 0;

    if (sc == SC_Q) {
        /* Quit - signal we want to exit via game_over without drawing */
        game_over = 1;
        return;
    }

    if (game_over) {
        if (sc == SC_R) {
            reset();
        }
        return;
    }

    /* Jump triggers */
    if (sc == SC_SPACE || sc == SC_W ||
        (ext && sc == SC_UP_RAW)) {
        if (!jumping && dino_y >= GROUND_Y) {
            jumping = 1;
            dino_vy = JUMP_VEL;
        }
    }
}

/* ── Game over screen: wait for R to restart or Q to quit ─────── */
static int game_over_loop(void) {
    while (1) {
        uint32_t key = read_key();
        if (key) {
            uint8_t sc = key & 0x7F;
            if (sc == SC_Q) return 0;   /* quit */
            if (sc == SC_R) return 1;   /* restart */
        }
        sleep_ms(50);
    }
}

/* ── Main ─────────────────────────────────────────────────────── */
int main(void) {
    reset();

    while (1) {
        /* Render current frame */
        draw();

        /* Handle game over state */
        if (game_over) {
            /* Check if user quit via Q key during gameplay */
            /* game_over_loop returns 1 for restart, 0 for quit */
            if (!game_over_loop())
                break;
            continue;
        }

        /* Non-blocking read: process one keypress per frame */
        uint32_t key = read_key();
        if (key) {
            handle_key(key);
            /* If Q was pressed, break out */
            if (game_over) {
                /* Q was pressed, confirm exit */
                clear_screen();
                printf("=== TinyOS Dino ===\n");
                printf("Final score: %d\n", score);
                printf("Thanks for playing!\n");
                return 0;
            }
        }

        /* Advance game state by one frame */
        advance();

        /* Frame timing */
        sleep_ms(GAME_TICK);
    }

    clear_screen();
    printf("=== TinyOS Dino ===\n");
    printf("Final score: %d\n", score);
    printf("Thanks for playing!\n");
    return 0;
}
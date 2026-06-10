/* si.c - Space Invaders for TinyOS (VGA text mode)
 * A simplified Space Invaders clone using ASCII characters.
 *
 * Controls:
 *   LEFT/RIGHT or A/D  - Move cannon
 *   SPACE              - Fire
 *   Q                  - Quit
 *   R                  - Restart (after game over)
 *
 * Characters:
 *   A  = Alien (invader)
 *   ^  = Player bullet
 *   v  = Alien bomb
 *   T  = Player cannon (turret)
 *   #  = Shield block
 */

#include <stdio.h>
#include <syscall.h>
#include <unistd.h>

/* ── Screen / playfield ─────────────────────────────────────── */
#define FIELD_W     60
#define FIELD_H     20
#define GROUND_ROW  (FIELD_H - 1)

/* ── Scancodes (PS/2 set 1) ─────────────────────────────────── */
#define SC_LEFT     0x4B
#define SC_RIGHT    0x4D
#define SC_A        0x1E
#define SC_D        0x20
#define SC_SPACE    0x39
#define SC_Q        0x10
#define SC_R        0x13
#define SC_UP       0x48

/* ── Limits ─────────────────────────────────────────────────── */
#define MAX_ALIENS     30
#define MAX_BULLETS     4
#define MAX_BOMBS       3
#define MAX_SHIELDS    20
#define ALIEN_ROWS      3
#define ALIEN_COLS     10

/* ── Types ──────────────────────────────────────────────────── */
typedef struct { int x, y; int alive; } Entity;

/* ── Game state ─────────────────────────────────────────────── */
static Entity aliens[MAX_ALIENS];
static Entity bullets[MAX_BULLETS];
static Entity bombs[MAX_BOMBS];
static Entity shields[MAX_SHIELDS];

static int cannon_x;
static int score;
static int lives;
static int game_over;
static int won;
static int frame;
static int alien_dir;       /* +1 = right, -1 = left */
static int alien_step_cd;   /* countdown between alien moves */
static int fire_cd;         /* cooldown between shots */

/* ── Field buffer ───────────────────────────────────────────── */
static char field[FIELD_H][FIELD_W];

/* ── Helpers ────────────────────────────────────────────────── */
static void clear_field(void) {
    for (int y = 0; y < FIELD_H; y++)
        for (int x = 0; x < FIELD_W; x++)
            field[y][x] = ' ';
}

static void put(int x, int y, char c) {
    if (x >= 0 && x < FIELD_W && y >= 0 && y < FIELD_H)
        field[y][x] = c;
}

/* ── Init / reset ───────────────────────────────────────────── */
static void reset(void) {
    cannon_x = FIELD_W / 2;
    score = 0;
    lives = 3;
    game_over = 0;
    won = 0;
    frame = 0;
    alien_dir = 1;
    alien_step_cd = 8;
    fire_cd = 0;

    /* Place aliens in a grid */
    int idx = 0;
    for (int r = 0; r < ALIEN_ROWS; r++) {
        for (int c = 0; c < ALIEN_COLS; c++) {
            aliens[idx].x = 5 + c * 5;
            aliens[idx].y = 1 + r * 2;
            aliens[idx].alive = 1;
            idx++;
        }
    }
    /* Zero out remaining slots */
    for (int i = idx; i < MAX_ALIENS; i++)
        aliens[i].alive = 0;

    for (int i = 0; i < MAX_BULLETS; i++) bullets[i].alive = 0;
    for (int i = 0; i < MAX_BOMBS; i++)   bombs[i].alive = 0;

    /* Build shield line */
    int si = 0;
    for (int s = 0; s < 4; s++) {
        int bx = 6 + s * 14;
        for (int dx = 0; dx < 5 && si < MAX_SHIELDS; dx++) {
            shields[si].x = bx + dx;
            shields[si].y = GROUND_ROW - 3;
            shields[si].alive = 1;
            si++;
        }
    }
    for (int i = si; i < MAX_SHIELDS; i++) shields[i].alive = 0;
}

/* ── Count living aliens ────────────────────────────────────── */
static int aliens_alive(void) {
    int n = 0;
    for (int i = 0; i < MAX_ALIENS; i++)
        if (aliens[i].alive) n++;
    return n;
}

/* ── Fire a bullet from the cannon ──────────────────────────── */
static void fire(void) {
    if (fire_cd > 0) return;
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].alive) {
            bullets[i].x = cannon_x;
            bullets[i].y = GROUND_ROW - 1;
            bullets[i].alive = 1;
            fire_cd = 6;
            return;
        }
    }
}

/* ── Drop a bomb from a random alien ────────────────────────── */
static void alien_fire(void) {
    /* Find a random living alien */
    int candidates[MAX_ALIENS];
    int n = 0;
    for (int i = 0; i < MAX_ALIENS; i++)
        if (aliens[i].alive) candidates[n++] = i;
    if (n == 0) return;

    /* Simple pseudo-random using frame */
    int pick = candidates[(frame * 7 + 13) % n];
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].alive) {
            bombs[i].x = aliens[pick].x;
            bombs[i].y = aliens[pick].y + 1;
            bombs[i].alive = 1;
            return;
        }
    }
}

/* ── Move aliens left/right and step down ───────────────────── */
static void move_aliens(void) {
    alien_step_cd--;
    if (alien_step_cd > 0) return;

    /* Speed up as fewer aliens remain */
    int alive = aliens_alive();
    alien_step_cd = (alive > 20) ? 8 : (alive > 10) ? 5 : (alive > 5) ? 3 : 2;

    /* Check if any alien hits the edge */
    int hit_edge = 0;
    for (int i = 0; i < MAX_ALIENS; i++) {
        if (!aliens[i].alive) continue;
        if (alien_dir > 0 && aliens[i].x >= FIELD_W - 2) hit_edge = 1;
        if (alien_dir < 0 && aliens[i].x <= 1) hit_edge = 1;
    }

    if (hit_edge) {
        /* Reverse direction and step down */
        alien_dir = -alien_dir;
        for (int i = 0; i < MAX_ALIENS; i++) {
            if (aliens[i].alive) aliens[i].y++;
        }
    } else {
        for (int i = 0; i < MAX_ALIENS; i++) {
            if (aliens[i].alive) aliens[i].x += alien_dir;
        }
    }
}

/* ── Collision checks ───────────────────────────────────────── */
static void check_collisions(void) {
    /* Bullets vs aliens */
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].alive) continue;
        for (int a = 0; a < MAX_ALIENS; a++) {
            if (!aliens[a].alive) continue;
            if (bullets[b].x == aliens[a].x && bullets[b].y == aliens[a].y) {
                aliens[a].alive = 0;
                bullets[b].alive = 0;
                score += 10;
                break;
            }
        }
    }

    /* Bullets vs shields */
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (!bullets[b].alive) continue;
        for (int s = 0; s < MAX_SHIELDS; s++) {
            if (!shields[s].alive) continue;
            if (bullets[b].x == shields[s].x && bullets[b].y == shields[s].y) {
                shields[s].alive = 0;
                bullets[b].alive = 0;
                break;
            }
        }
    }

    /* Bombs vs cannon */
    for (int b = 0; b < MAX_BOMBS; b++) {
        if (!bombs[b].alive) continue;
        if (bombs[b].y >= GROUND_ROW - 1 &&
            bombs[b].x >= cannon_x - 1 && bombs[b].x <= cannon_x + 1) {
            bombs[b].alive = 0;
            lives--;
            if (lives <= 0) {
                game_over = 1;
            }
        }
    }

    /* Bombs vs shields */
    for (int b = 0; b < MAX_BOMBS; b++) {
        if (!bombs[b].alive) continue;
        for (int s = 0; s < MAX_SHIELDS; s++) {
            if (!shields[s].alive) continue;
            if (bombs[b].x == shields[s].x && bombs[b].y == shields[s].y) {
                shields[s].alive = 0;
                bombs[b].alive = 0;
                break;
            }
        }
    }

    /* Aliens reaching ground */
    for (int a = 0; a < MAX_ALIENS; a++) {
        if (aliens[a].alive && aliens[a].y >= GROUND_ROW - 2) {
            game_over = 1;
        }
    }

    /* All aliens dead = win */
    if (aliens_alive() == 0) {
        won = 1;
        game_over = 1;
    }
}

/* ── Advance one frame ──────────────────────────────────────── */
static void advance(void) {
    if (game_over) return;
    frame++;
    if (fire_cd > 0) fire_cd--;

    /* Move bullets up */
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].alive) {
            bullets[i].y--;
            if (bullets[i].y < 0) bullets[i].alive = 0;
        }
    }

    /* Move bombs down */
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (bombs[i].alive) {
            bombs[i].y++;
            if (bombs[i].y >= FIELD_H) bombs[i].alive = 0;
        }
    }

    /* Alien movement */
    move_aliens();

    /* Alien fires periodically */
    if (frame % 12 == 0) alien_fire();

    check_collisions();
}

/* ── Render to screen ───────────────────────────────────────── */
static void draw(void) {
    clear_field();

    /* Draw aliens */
    for (int i = 0; i < MAX_ALIENS; i++)
        if (aliens[i].alive) put(aliens[i].x, aliens[i].y, 'A');

    /* Draw bullets */
    for (int i = 0; i < MAX_BULLETS; i++)
        if (bullets[i].alive) put(bullets[i].x, bullets[i].y, '^');

    /* Draw bombs */
    for (int i = 0; i < MAX_BOMBS; i++)
        if (bombs[i].alive) put(bombs[i].x, bombs[i].y, 'v');

    /* Draw shields */
    for (int i = 0; i < MAX_SHIELDS; i++)
        if (shields[i].alive) put(shields[i].x, shields[i].y, '#');

    /* Draw cannon */
    put(cannon_x, GROUND_ROW, 'T');
    if (cannon_x > 0)         put(cannon_x - 1, GROUND_ROW, '=');
    if (cannon_x < FIELD_W-1) put(cannon_x + 1, GROUND_ROW, '=');

    /* Output to terminal */
    clear_screen();
    printf("=== SI: Space Invaders ===  Score: %d  Lives: %d\n", score, lives);

    if (game_over) {
        if (won)
            printf("  *** YOU WIN! ***  Press R=Restart  Q=Quit\n");
        else
            printf("  *** GAME OVER! ***  Press R=Restart  Q=Quit\n");
    } else {
        printf("  A/D or LEFT/RIGHT=Move  SPACE=Fire  Q=Quit\n");
    }

    /* Top border */
    putchar('+');
    for (int x = 0; x < FIELD_W; x++) putchar('-');
    putchar('+');
    putchar('\n');

    /* Field rows */
    for (int y = 0; y < FIELD_H; y++) {
        putchar('|');
        for (int x = 0; x < FIELD_W; x++)
            putchar(field[y][x]);
        putchar('|');
        putchar('\n');
    }

    /* Bottom border */
    putchar('+');
    for (int x = 0; x < FIELD_W; x++) putchar('-');
    putchar('+');
    putchar('\n');
}

/* ── Handle input ───────────────────────────────────────────── */
static void handle_key(uint32_t raw) {
    uint8_t sc = raw & 0x7F;
    uint8_t release = (raw & 0x80) ? 1 : 0;
    if (release) return; /* ignore key releases */

    if (sc == SC_Q) {
        game_over = 1;
        won = 0;
        return;
    }

    if (game_over) {
        if (sc == SC_R) reset();
        return;
    }

    if (sc == SC_LEFT || sc == SC_A) {
        if (cannon_x > 1) cannon_x--;
    } else if (sc == SC_RIGHT || sc == SC_D) {
        if (cannon_x < FIELD_W - 2) cannon_x++;
    } else if (sc == SC_SPACE) {
        fire();
    }
}

/* ── Main ───────────────────────────────────────────────────── */
int main(void) {
    reset();

    while (1) {
        draw();

        if (game_over) {
            /* Wait for R or Q */
            while (1) {
                uint32_t key = read_key();
                if (key) {
                    uint8_t sc = key & 0x7F;
                    uint8_t rel = (key & 0x80) ? 1 : 0;
                    if (!rel) {
                        if (sc == SC_Q) goto done;
                        if (sc == SC_R) { reset(); break; }
                    }
                }
                sleep_ms(50);
            }
            continue;
        }

        /* Non-blocking key read */
        uint32_t key = read_key();
        if (key) handle_key(key);

        advance();
        sleep_ms(60);
    }

done:
    clear_screen();
    printf("=== SI: Space Invaders ===\n");
    printf("Final score: %d\n", score);
    printf("Thanks for playing!\n");
    return 0;
}

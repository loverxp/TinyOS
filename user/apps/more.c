/* more.c - Pager for viewing long output one screen at a time
 *
 * Usage:
 *   command | more    -- read from stdin (pipe)
 *   more <file>       -- read from file
 *
 * Controls (at --More-- prompt):
 *   SPACE  - next page
 *   ENTER  - next line
 *   Q      - quit
 *
 * Key input: Uses read_char_nonblock() which reads from the keyboard
 * character buffer (char_buffer). This buffer is populated by the IRQ1
 * keyboard handler when char_callback is NULL (during user program execution).
 * Works in both VGA/PS2 and serial/nographic modes.
 */

#include <stdio.h>
#include <syscall.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#define LINES_PER_PAGE 22  /* 25 VGA rows - 2 (status) - 1 (prompt) */
#define MAX_LINES     512  /* max lines we can buffer */

int main(void) {
    char args[256];
    get_cmdline(args, sizeof(args));

    int from_file = (args[0] != '\0');
    int fd = -1;

    if (from_file) {
        fd = open(args, O_RDONLY);
        if (fd < 0) {
            printf("File not found: %s\n", args);
            return 1;
        }
    }

    /* ── Phase 1: Read all input into line buffer ── */
    char* lines[MAX_LINES];
    int total_lines = 0;
    char line_buf[256];
    int pos = 0;

    while (1) {
        int ch;
        if (from_file) {
            char c;
            int n = read(fd, &c, 1);
            if (n <= 0) break;
            ch = (unsigned char)c;
        } else {
            ch = getchar();
            if (ch <= 0) break;
        }

        if (ch == '\n') {
            line_buf[pos] = '\0';
            char* copy = malloc(pos + 1);
            if (!copy) break;
            for (int i = 0; i < pos; i++) copy[i] = line_buf[i];
            copy[pos] = '\0';
            if (total_lines < MAX_LINES) {
                lines[total_lines++] = copy;
            } else {
                free(copy);
                break;
            }
            pos = 0;
        } else if (ch >= 32 && ch < 127) {
            if (pos < 254) line_buf[pos++] = (char)ch;
        }
    }
    /* Flush partial last line */
    if (pos > 0) {
        line_buf[pos] = '\0';
        char* copy = malloc(pos + 1);
        if (copy) {
            for (int i = 0; i < pos; i++) copy[i] = line_buf[i];
            copy[pos] = '\0';
            if (total_lines < MAX_LINES) lines[total_lines++] = copy;
            else free(copy);
        }
    }

    /* ── Phase 2: Paginate ── */
    int line_idx = 0;
    while (line_idx < total_lines) {
        /* Display one page */
        for (int i = 0; i < LINES_PER_PAGE && line_idx < total_lines; i++, line_idx++) {
            printf("%s\n", lines[line_idx]);
        }
        if (line_idx >= total_lines) break;

        /* Show pager prompt */
        printf("--More-- (SPACE=next, ENTER=line, Q=quit)");

        int advance_lines = 0;
        int spinner = 0;
        while (1) {
            /* Poll for character input.
             * Works for both PS/2 keyboard (via IRQ1 handler → char_buffer)
             * and serial/nographic mode (via serial IRQ handler → char_buffer).
             */
            int ch = read_char_nonblock();
            if (ch > 0) {
                if (ch == 'q' || ch == 'Q') {
                    printf("\n");
                    goto done;
                }
                if (ch == ' ') {
                    advance_lines = LINES_PER_PAGE;
                    break;
                }
                if (ch == '\n' || ch == '\r') {
                    advance_lines = 1;
                    break;
                }
                /* Unknown char — just ignore and continue */
            }

            /* Show a spinner to indicate the program is alive */
            spinner = (spinner + 1) % 4;
            char spin_ch = "|/-\\"[spinner];
            printf("\r%c --More-- (SPACE=next, ENTER=line, Q=quit)", spin_ch);

            sleep_ms(50);
        }

        /* Erase the --More-- prompt */
        printf("\r                                         \r");

        if (advance_lines == 1) {
            /* ENTER: back up so only 1 line advances */
            line_idx -= (LINES_PER_PAGE - 1);
        }
        /* SPACE: continue normally */
    }

done:
    /* Free allocated lines */
    for (int i = 0; i < total_lines; i++) {
        free(lines[i]);
    }
    if (from_file) close(fd);
    return 0;
}
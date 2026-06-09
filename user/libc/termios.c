/* termios.c - POSIX terminal I/O stubs for TinyOS
 * All functions return a fixed "raw mode" configuration.
 * tcgetattr always succeeds with raw-mode settings.
 * tcsetattr silently accepts any settings (no-op).
 */

#include <termios.h>
#include <string.h>

/* Fill termios struct with "raw mode" defaults */
static void fill_raw(struct termios* t) {
    memset(t, 0, sizeof(*t));
    t->c_iflag = 0;           /* no input processing */
    t->c_oflag = 0;           /* no output processing */
    t->c_cflag = CS8 | CREAD | CLOCAL;  /* 8-bit, enable rx, local */
    t->c_lflag = 0;           /* no echo, no canonical, no signals */
    t->c_cc[VMIN]  = 1;
    t->c_cc[VTIME] = 0;
    t->c_ispeed = B115200;
    t->c_ospeed = B115200;
}

int tcgetattr(int fd, struct termios* termios_p) {
    (void)fd;
    if (!termios_p) return -1;
    fill_raw(termios_p);
    return 0;
}

int tcsetattr(int fd, int optional_actions, const struct termios* termios_p) {
    (void)fd;
    (void)optional_actions;
    (void)termios_p;
    return 0;  /* silently accept */
}

speed_t cfgetispeed(const struct termios* termios_p) {
    return termios_p ? termios_p->c_ispeed : B115200;
}

speed_t cfgetospeed(const struct termios* termios_p) {
    return termios_p ? termios_p->c_ospeed : B115200;
}

int cfsetispeed(struct termios* termios_p, speed_t speed) {
    if (termios_p) termios_p->c_ispeed = speed;
    return 0;
}

int cfsetospeed(struct termios* termios_p, speed_t speed) {
    if (termios_p) termios_p->c_ospeed = speed;
    return 0;
}

void cfmakeraw(struct termios* termios_p) {
    if (termios_p) fill_raw(termios_p);
}

int tcflush(int fd, int queue_selector) {
    (void)fd;
    (void)queue_selector;
    return 0;
}

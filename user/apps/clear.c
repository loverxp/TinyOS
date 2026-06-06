/* clear.c - User-space clear screen command */

#include "../libc/syscall.h"

int main(void) {
    clear_screen();
    return 0;
}
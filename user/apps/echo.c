/* echo.c - User-space echo command
 * Reads arguments from syscall 23 (get_cmdline) and prints them.
 */

#include "../libc/stdio.h"
#include "../libc/syscall.h"

int main(void) {
    char args[256];
    int len = get_cmdline(args, sizeof(args));
    
    if (len > 0) {
        /* Print the echo text */
        printf("%s\n", args);
    }
    
    return 0;
}
/* rand.c - User-space random number command */

#include "../libc/syscall.h"
#include "../libc/stdio.h"

int main(void) {
    uint32_t val;
    if (get_system_info(2, &val, sizeof(val)) < sizeof(val)) {
        printf("rand: system call failed\n");
        return 1;
    }
    printf("%u\n", val);
    return 0;
}
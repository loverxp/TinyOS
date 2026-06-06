/* date.c - User-space date/time command */

#include "../libc/syscall.h"
#include "../libc/stdio.h"

typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
} rtc_time_t;

int main(void) {
    rtc_time_t tm;
    if (get_system_info(1, &tm, sizeof(tm)) < sizeof(tm)) {
        printf("date: system call failed\n");
        return 1;
    }
    printf("%04u-%02u-%02u %02u:%02u:%02u\n",
           tm.year, tm.month, tm.day,
           tm.hour, tm.minute, tm.second);
    return 0;
}
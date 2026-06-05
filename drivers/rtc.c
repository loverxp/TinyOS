#include "../include/rtc.h"
#include "../include/io.h"

/* CMOS I/O ports */
#define CMOS_INDEX  0x70
#define CMOS_DATA   0x71

/* CMOS registers */
#define RTC_SECONDS    0x00
#define RTC_MINUTES    0x02
#define RTC_HOURS      0x04
#define RTC_DAY        0x07
#define RTC_MONTH      0x08
#define RTC_YEAR       0x09
#define RTC_CENTURY    0x32  /* May not be supported on all machines */
#define RTC_STATUS_A   0x0A
#define RTC_STATUS_B   0x0B

/* Status Register A bits */
#define RTC_UIP  0x80  /* Update In Progress */

/* Status Register B bits */
#define RTC_BCD  0x04  /* 0 = BCD mode, 1 = binary mode */
#define RTC_24H  0x02  /* 0 = 12-hour, 1 = 24-hour */

/* Read a value from CMOS */
static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_INDEX, reg);
    io_wait();
    return inb(CMOS_DATA);
}

/* Convert BCD to binary */
static uint8_t bcd_to_bin(uint8_t bcd) {
    return (bcd & 0x0F) + ((bcd >> 4) * 10);
}

void rtc_read_time(rtc_time_t* tm) {
    /* Wait for RTC to not be updating (UIP=0) */
    while (cmos_read(RTC_STATUS_A) & RTC_UIP);

    /* Check if RTC is in BCD or binary mode */
    uint8_t status_b = cmos_read(RTC_STATUS_B);
    int is_bcd = !(status_b & RTC_BCD);

    /* Read all registers */
    uint8_t sec   = cmos_read(RTC_SECONDS);
    uint8_t min   = cmos_read(RTC_MINUTES);
    uint8_t hour  = cmos_read(RTC_HOURS);
    uint8_t day   = cmos_read(RTC_DAY);
    uint8_t mon   = cmos_read(RTC_MONTH);
    uint8_t year  = cmos_read(RTC_YEAR);
    uint8_t century = cmos_read(RTC_CENTURY);

    /* Convert BCD to binary if needed */
    if (is_bcd) {
        sec   = bcd_to_bin(sec);
        min   = bcd_to_bin(min);
        hour  = bcd_to_bin(hour);
        day   = bcd_to_bin(day);
        mon   = bcd_to_bin(mon);
        year  = bcd_to_bin(year);
        century = bcd_to_bin(century);
    }

    tm->second = sec;
    tm->minute = min;
    tm->hour   = hour;
    tm->day    = day;
    tm->month  = mon;

    /* Handle century (QEMY typically supports this) */
    if (century >= 19) {
        tm->year = (uint16_t)century * 100 + year;
    } else {
        tm->year = 2000 + year;
    }
}
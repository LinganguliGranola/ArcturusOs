#include "TimeCommands.h"

#include "../terminal.h"
#include "../../drivers/io.h"

#include <stdint.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

struct rtc_time {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint8_t year;
};

static uint32_t boot_time;

static uint8_t cmos_read(uint8_t reg) {
    outb(CMOS_ADDRESS, (uint8_t)(reg | 0x80));
    return inb(CMOS_DATA);
}

static uint8_t bcd_to_binary(uint8_t value) {
    return (uint8_t)((value & 0x0F) + ((value >> 4) * 10));
}

static void read_rtc(struct rtc_time *time) {
    uint8_t status_b;
    struct rtc_time first;
    struct rtc_time second;

    while (cmos_read(0x0A) & 0x80) {
    }
    status_b = cmos_read(0x0B);
    do {
        first.second = cmos_read(0x00);
        first.minute = cmos_read(0x02);
        first.hour = cmos_read(0x04);
        first.day = cmos_read(0x07);
        first.month = cmos_read(0x08);
        first.year = cmos_read(0x09);
        while (cmos_read(0x0A) & 0x80) {
        }
        second.second = cmos_read(0x00);
        second.minute = cmos_read(0x02);
        second.hour = cmos_read(0x04);
        second.day = cmos_read(0x07);
        second.month = cmos_read(0x08);
        second.year = cmos_read(0x09);
    } while (first.second != second.second || first.minute != second.minute ||
             first.hour != second.hour || first.day != second.day ||
             first.month != second.month || first.year != second.year);

    *time = second;
    if (!(status_b & 0x04)) {
        time->second = bcd_to_binary(time->second);
        time->minute = bcd_to_binary(time->minute);
        time->hour = bcd_to_binary((uint8_t)(time->hour & 0x7F));
        time->day = bcd_to_binary(time->day);
        time->month = bcd_to_binary(time->month);
        time->year = bcd_to_binary(time->year);
    }
    if (!(status_b & 0x02)) {
        uint8_t is_pm = second.hour & 0x80;

        time->hour &= 0x7F;
        if (time->hour == 12)
            time->hour = 0;
        if (is_pm)
            time->hour = (uint8_t)(time->hour + 12);
    }
}

static int is_leap_year(uint16_t year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

static uint32_t rtc_seconds(const struct rtc_time *time) {
    static const uint8_t month_days[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    uint16_t year = (uint16_t)(2000 + time->year);
    uint32_t days = 0;
    uint16_t current_year;
    uint8_t month;

    for (current_year = 2000; current_year < year; current_year++)
        days += (uint32_t)(365 + is_leap_year(current_year));
    for (month = 1; month < time->month; month++) {
        days += month_days[month - 1];
        if (month == 2 && is_leap_year(year))
            days++;
    }
    days += (uint32_t)(time->day - 1);
    return days * 86400U + (uint32_t)time->hour * 3600U +
           (uint32_t)time->minute * 60U + time->second;
}

static void write_two_digits(uint8_t value) {
    terminal_putchar((char)('0' + value / 10));
    terminal_putchar((char)('0' + value % 10));
}

static void write_unsigned(uint32_t value) {
    char digits[10];
    uint8_t count = 0;

    do {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    while (count != 0)
        terminal_putchar(digits[--count]);
}

static int text_equal(const char *left, const char *right) {
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }
    return *left == *right;
}

void time_commands_initialize(void) {
    struct rtc_time time;

    read_rtc(&time);
    boot_time = rtc_seconds(&time);
}

bool time_commands_execute(const char *command) {
    struct rtc_time time;
    uint32_t elapsed;

    if (text_equal(command, "time")) {
        read_rtc(&time);
        write_two_digits(time.hour);
        terminal_putchar(':');
        write_two_digits(time.minute);
        terminal_putchar(':');
        write_two_digits(time.second);
        terminal_putchar('\n');
        return true;
    }
    if (!text_equal(command, "uptime"))
        return false;

    read_rtc(&time);
    elapsed = rtc_seconds(&time) - boot_time;
    terminal_writestring("up ");
    write_unsigned(elapsed / 86400U);
    terminal_writestring(" days, ");
    write_two_digits((uint8_t)((elapsed / 3600U) % 24U));
    terminal_putchar(':');
    write_two_digits((uint8_t)((elapsed / 60U) % 60U));
    terminal_putchar(':');
    write_two_digits((uint8_t)(elapsed % 60U));
    terminal_putchar('\n');
    return true;
}

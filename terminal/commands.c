#include "commands.h"
#include "terminal.h"
#include "../drivers/io.h"
#include "../filesystem/filesystem.h"

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
    /* Setting bit 7 prevents an NMI while selecting the CMOS register. */
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

    /* Do not sample while the RTC is copying its counters to the registers. */
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

    /* Register B bit 2 selects binary values; otherwise the RTC uses BCD. */
    if (!(status_b & 0x04)) {
        time->second = bcd_to_binary(time->second);
        time->minute = bcd_to_binary(time->minute);
        time->hour = bcd_to_binary((uint8_t)(time->hour & 0x7F));
        time->day = bcd_to_binary(time->day);
        time->month = bcd_to_binary(time->month);
        time->year = bcd_to_binary(time->year);
    }

    /* Register B bit 1 clear means a 12-hour clock; bit 7 denotes PM. */
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
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}

static uint32_t rtc_seconds(const struct rtc_time *time) {
    static const uint8_t month_days[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    uint16_t year = (uint16_t)(2000 + time->year);
    uint32_t days = 0;

    for (uint16_t current_year = 2000; current_year < year; current_year++)
        days += (uint32_t)(365 + is_leap_year(current_year));
    for (uint8_t month = 1; month < time->month; month++) {
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

static int starts_with(const char *text, const char *prefix) {
    while (*prefix != '\0') {
        if (*text++ != *prefix++)
            return 0;
    }
    return 1;
}

static const char *skip_spaces(const char *text) {
    while (*text == ' ')
        text++;
    return text;
}

static int split_argument(const char *text, char *argument, uint8_t capacity,
                          const char **remainder) {
    uint8_t length = 0;

    text = skip_spaces(text);
    if (*text == '\0')
        return 0;
    while (*text != '\0' && *text != ' ') {
        if (length + 1 >= capacity)
            return 0;
        argument[length++] = *text++;
    }
    argument[length] = '\0';
    *remainder = skip_spaces(text);
    return 1;
}

static void command_help(void) {
    terminal_writestring("Filesystem commands:\n");
    terminal_writestring("  mounts                 list disk mount paths\n");
    terminal_writestring("  ls <path>              list a mount or directory\n");
    terminal_writestring("  mkdir <path>           create a directory\n");
    terminal_writestring("  touch <path>           create an empty file\n");
    terminal_writestring("  write <path> <text>    replace file contents\n");
    terminal_writestring("  append <path> <text>   append to a file\n");
    terminal_writestring("  cat <path>             show a file\n");
    terminal_writestring("  rm <path>              remove an empty directory or file\n");
}

static bool execute_filesystem_command(const char *command) {
    char path[56];
    const char *remainder;

    if (text_equal(command, "mounts")) {
        filesystem_print_mounts();
        return true;
    }
    if (text_equal(command, "help")) {
        command_help();
        return true;
    }
    if (starts_with(command, "ls ")) {
        if (!split_argument(command + 3, path, sizeof(path), &remainder) ||
            *remainder != '\0' || !filesystem_list(path))
            terminal_writestring("Path not found.\n");
        return true;
    }
    if (starts_with(command, "mkdir ")) {
        if (!split_argument(command + 6, path, sizeof(path), &remainder) ||
            *remainder != '\0' || !filesystem_make_directory(path))
            terminal_writestring("Could not create directory.\n");
        return true;
    }
    if (starts_with(command, "touch ")) {
        if (!split_argument(command + 6, path, sizeof(path), &remainder) ||
            *remainder != '\0' || !filesystem_create_file(path))
            terminal_writestring("Could not create file.\n");
        return true;
    }
    if (starts_with(command, "cat ")) {
        if (!split_argument(command + 4, path, sizeof(path), &remainder) ||
            *remainder != '\0' || !filesystem_read_file(path))
            terminal_writestring("File not found.\n");
        return true;
    }
    if (starts_with(command, "rm ")) {
        if (!split_argument(command + 3, path, sizeof(path), &remainder) ||
            *remainder != '\0' || !filesystem_remove(path))
            terminal_writestring("Could not remove path.\n");
        return true;
    }
    if (starts_with(command, "write ") || starts_with(command, "append ")) {
        uint8_t append = command[0] == 'a';
        const char *arguments = command + (append ? 7 : 6);

        if (!split_argument(arguments, path, sizeof(path), &remainder) ||
            *remainder == '\0' ||
            !filesystem_write_file(path, remainder, append != 0))
            terminal_writestring("Could not write file.\n");
        return true;
    }
    return false;
}

void commands_initialize(void) {
    struct rtc_time time;

    read_rtc(&time);
    boot_time = rtc_seconds(&time);
}

void command_time(void) {
    struct rtc_time time;

    read_rtc(&time);
    write_two_digits(time.hour);
    terminal_putchar(':');
    write_two_digits(time.minute);
    terminal_putchar(':');
    write_two_digits(time.second);
    terminal_putchar('\n');
}

void command_uptime(void) {
    struct rtc_time time;
    uint32_t elapsed;

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
}

bool commands_execute(const char *command) {
    const char *time_command = "time";
    const char *uptime_command = "uptime";
    uint8_t index = 0;

    while (command[index] == time_command[index] && command[index] != '\0')
        index++;
    if (command[index] == time_command[index]) {
        command_time();
        return true;
    }

    index = 0;
    while (command[index] == uptime_command[index] && command[index] != '\0')
        index++;
    if (command[index] == uptime_command[index]) {
        command_uptime();
        return true;
    }

    return execute_filesystem_command(command);
}

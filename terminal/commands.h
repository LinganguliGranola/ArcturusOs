#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>

/* Capture the RTC value used as the reference point for uptime. */
void commands_initialize(void);

/* Print the current RTC time as HH:MM:SS. */
void command_time(void);

/* Print the elapsed time since commands_initialize(). */
void command_uptime(void);

/* Run a command line.  Returns false when the command is not recognised. */
bool commands_execute(const char *command);

#endif

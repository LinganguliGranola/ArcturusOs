#ifndef TIME_COMMANDS_H
#define TIME_COMMANDS_H

#include <stdbool.h>

void time_commands_initialize(void);
bool time_commands_execute(const char *command);

#endif

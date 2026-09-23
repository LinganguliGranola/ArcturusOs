#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdbool.h>

void commands_initialize(void);
bool commands_execute(const char *command);
const char *commands_current_path(void);

#endif

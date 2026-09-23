#ifndef DIRECTORY_COMMANDS_H
#define DIRECTORY_COMMANDS_H

#include <stdbool.h>

void directory_commands_initialize(void);
bool directory_commands_execute(const char *command);
const char *directory_commands_current_path(void);

#endif

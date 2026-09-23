#include "Commands.h"

#include "DirectoryCommands.h"
#include "TimeCommands.h"

void commands_initialize(void) {
    time_commands_initialize();
    directory_commands_initialize();
}

bool commands_execute(const char *command) {
    if (time_commands_execute(command))
        return true;
    return directory_commands_execute(command);
}

const char *commands_current_path(void) {
    return directory_commands_current_path();
}

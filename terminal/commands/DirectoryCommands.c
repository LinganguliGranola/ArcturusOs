#include "DirectoryCommands.h"

#include "../../filesystem/filesystem.h"
#include "../terminal.h"

#include <stddef.h>
#include <stdint.h>

#define PATH_CAPACITY 56U

static char current_path[PATH_CAPACITY];
static const char home_path[] = "run/home";

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

/* Returns 1 for no argument, 2 for one argument, and 0 for invalid input. */
static int read_optional_argument(const char *text, char *argument,
                                  size_t capacity) {
  size_t length = 0;

  text = skip_spaces(text);
  if (*text == '\0') {
    argument[0] = '\0';
    return 1;
  }
  while (*text != '\0' && *text != ' ') {
    if (length + 1 >= capacity)
      return 0;
    argument[length++] = *text++;
  }
  if (*skip_spaces(text) != '\0')
    return 0;
  argument[length] = '\0';
  return 2;
}

static int append_component(char *path, size_t *length, const char *component,
                            size_t component_length) {
  size_t index;

  if (*length != 1) {
    if (*length + 1 >= PATH_CAPACITY)
      return 0;
    path[(*length)++] = '/';
  }
  if (*length + component_length >= PATH_CAPACITY)
    return 0;
  for (index = 0; index < component_length; index++)
    path[(*length)++] = component[index];
  path[*length] = '\0';
  return 1;
}

static void remove_last_component(char *path, size_t *length) {
  if (*length <= 1)
    return;
  while (*length > 0 && path[*length - 1] != '/')
    (*length)--;
  if (*length > 1)
    (*length)--;
  else if (*length == 1 && path[0] != '/')
    (*length) = 0;
  path[*length] = '\0';
}

/* Resolve absolute and relative paths without permitting traversal above /. */
static int resolve_path(const char *input, char output[PATH_CAPACITY]) {
  const char *cursor = input;
  size_t length = 0;

  if (cursor[0] == '~' && (cursor[1] == '\0' || cursor[1] == '/')) {
    while (length + 1 < PATH_CAPACITY && home_path[length] != '\0') {
      output[length] = home_path[length];
      length++;
    }
    if (home_path[length] != '\0')
      return 0;
    cursor++;
  } else if (*cursor == '/') {
    output[length++] = '/';
    cursor++;
  } else {
    while (length + 1 < PATH_CAPACITY && current_path[length] != '\0') {
      output[length] = current_path[length];
      length++;
    }
    if (current_path[length] != '\0')
      return 0;
  }

  while (*cursor != '\0') {
    const char *component;
    size_t component_length = 0;

    while (*cursor == '/')
      cursor++;
    if (*cursor == '\0')
      break;
    component = cursor;
    while (*cursor != '\0' && *cursor != '/') {
      component_length++;
      cursor++;
    }
    if (component_length == 1 && component[0] == '.')
      continue;
    if (component_length == 2 && component[0] == '.' && component[1] == '.') {
      remove_last_component(output, &length);
      continue;
    }
    if (!append_component(output, &length, component, component_length))
      return 0;
  }
  output[length] = '\0';
  return true;
}

const char *directory_commands_current_path(void) { return current_path; }

void directory_commands_initialize(void) {
  size_t index = 0;

  while (index + 1 < sizeof(current_path) && home_path[index] != '\0') {
    current_path[index] = home_path[index];
    index++;
  }
  current_path[index] = '\0';
}

bool directory_commands_execute(const char *command) {
  char argument[PATH_CAPACITY];
  char path[PATH_CAPACITY];
  size_t index;
  int argument_status;

  if (starts_with(command, "mkdir ")) {
    if (read_optional_argument(command + 6, argument, sizeof(argument)) != 2 ||
        !resolve_path(argument, path) || !filesystem_make_directory(path)) {
      terminal_writestring("mkdir: could not create directory\n");
    }
    return true;
  }
  if (starts_with(command, "touch ")) {
    if (read_optional_argument(command + 6, argument, sizeof(argument)) != 2 ||
        !resolve_path(argument, path) || !filesystem_create_file(path)) {
      terminal_writestring("touch: could not create file\n");
    }
    return true;
  }
  if (starts_with(command, "ls ") || starts_with(command, "ls")) {
    argument_status =
        read_optional_argument(command + 2, argument, sizeof(argument));
    if (argument_status == 1) {
      if (!filesystem_list(current_path))
        terminal_writestring("ls: directory not found\n");
      return true;
    } else if (argument_status != 2 || !resolve_path(argument, path)) {
      terminal_writestring("ls: directory not found\n");
      return true;
    }
    if (!filesystem_list(path)) {
      terminal_writestring("ls: directory not found\n");
    }
    return true;
  }
  if (starts_with(command, "cd ") || starts_with(command, "cd")) {
    argument_status =
        read_optional_argument(command + 2, argument, sizeof(argument));
    if (argument_status == 1) {
      if (!resolve_path("~", path))
        return true;
    } else if (argument_status != 2 || !resolve_path(argument, path)) {
      terminal_writestring("cd: directory not found\n");
      return true;
    }
    if (!filesystem_is_directory(path)) {
      terminal_writestring("cd: directory not found\n");
      return true;
    }
    for (index = 0; index < sizeof(current_path); index++) {
      current_path[index] = path[index];
      if (path[index] == '\0')
        break;
    }
    return true;
  }
  return false;
}

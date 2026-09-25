import sys

with open('./terminal/commands/DirectoryCommands.c', 'r') as f:
    content = f.read()

insert_code = """
  if (starts_with(command, "write ")) {
    char argument[PATH_CAPACITY];
    char path[PATH_CAPACITY];
    char content[128];
    const char *text = skip_spaces(command + 6);
    size_t length = 0;
    bool append = false;

    while (*text != '\\0' && *text != ' ') {
      if (length + 1 >= sizeof(argument)) break;
      argument[length++] = *text++;
    }
    argument[length] = '\\0';
    if (length == 0) {
      terminal_writestring("write: missing path\\n");
      return true;
    }
    if (!resolve_path(argument, path)) {
      terminal_writestring("write: invalid path\\n");
      return true;
    }

    text = skip_spaces(text);
    if (starts_with(text, ">>")) {
      append = true;
      text += 2;
    } else if (starts_with(text, ">")) {
      append = false;
      text += 1;
    } else {
      terminal_writestring("write: missing > or >>\\n");
      return true;
    }

    text = skip_spaces(text);
    length = 0;
    bool in_quotes = false;
    if (*text == '"') {
      in_quotes = true;
      text++;
    }
    while (*text != '\\0') {
      if (in_quotes && *text == '"' && *skip_spaces(text + 1) == '\\0') {
        break;
      }
      if (length + 1 >= sizeof(content)) break;
      content[length++] = *text++;
    }
    content[length] = '\\0';

    if (!filesystem_write_file(path, content, append)) {
      terminal_writestring("write: could not write file\\n");
    }
    return true;
  }
"""

replacement = '  if (starts_with(command, "touch ")) {' + insert_code

new_content = content.replace('  if (starts_with(command, "touch ")) {', replacement)

if new_content == content:
    print("Failed to replace!")
else:
    with open('./terminal/commands/DirectoryCommands.c', 'w') as f:
        f.write(new_content)
    print("Patched successfully")

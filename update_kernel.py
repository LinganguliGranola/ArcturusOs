import sys

with open('kernel/kernel.c', 'r') as f:
    content = f.read()

loop_start = content.find("    /* I/O loop */")
if loop_start != -1:
    new_loop = """    /* I/O loop */
    int input_length = 0;
    while (1) {
        char c = keyboard_read_char();
        if (c == 0) continue;

        if (c == '\\b') {
            if (input_length > 0) {
                terminal_backspace();
                input_length--;
            }
        } else if (c == '\\n') {
            terminal_putchar('\\n');
            terminal_writestring("> ");
            input_length = 0;
        } else {
            terminal_putchar(c);
            input_length++;
        }
    }
}"""
    content = content[:loop_start] + new_loop
    
    with open('kernel/kernel.c', 'w') as f:
        f.write(content)

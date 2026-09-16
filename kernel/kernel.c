#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../drivers/io.h"
#include "../drivers/keyboard.h"
#include "../terminal/terminal.h"
#include "../terminal/commands.h"
#include "../filesystem/filesystem.h"

void init_serial(void) {
    outb(0x3F8 + 1, 0x00);    // Disable all interrupts
    outb(0x3F8 + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    outb(0x3F8 + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    outb(0x3F8 + 1, 0x00);    //                  (hi byte)
    outb(0x3F8 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(0x3F8 + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    outb(0x3F8 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

static inline int is_transmit_empty(void) {
   return inb(0x3F8 + 5) & 0x20;
}

void serial_write_char(char c) {
   while (is_transmit_empty() == 0);
   outb(0x3F8, c);
}

void serial_write_string(const char* str) {
    for (size_t i = 0; str[i] != '\0'; i++) {
        serial_write_char(str[i]);
    }
}

void kernel_main(void) {
	/* Initialize terminal */
	terminal_initialize();
    init_serial();
    keyboard_init();
    commands_initialize();
    filesystem_initialize();
    serial_write_string("Filesystem initialization complete.\r\n");

	/* Print an inspiring message */
	terminal_setcolor(VGA_COLOR_LIGHT_CYAN | (VGA_COLOR_BLACK << 4));
	terminal_writestring("================================================================================\n");
	terminal_writestring("Welcome to ArcturusOs!\n");
	terminal_writestring("================================================================================\n");
	
	terminal_setcolor(VGA_COLOR_WHITE | (VGA_COLOR_BLACK << 4));
	terminal_writestring("Booted successfully into 32-bit protected mode.\n");
	terminal_writestring("Mini OS kernel developed in C11 and Assembly.\n");
	
	terminal_setcolor(VGA_COLOR_LIGHT_GREEN | (VGA_COLOR_BLACK << 4));
	terminal_writestring("Everything is up and running!\n");
    terminal_writestring("Hello from ArcturusOs! Serial output works.\n\n");

    terminal_writestring("Filesystem ready. Type 'help' for commands.\n");
    filesystem_print_mounts();
    terminal_putchar('\n');

    /* Prompt */
    terminal_setcolor(VGA_COLOR_LIGHT_GREY | (VGA_COLOR_BLACK << 4));
    terminal_writestring("> ");

    /* Also print to serial */
    serial_write_string("Hello from ArcturusOs! Serial output works.\r\n");

    /* I/O loop */
    char input[128];
    int input_length = 0;
    int input_cursor = 0;
    while (1) {
        int key = keyboard_read_key();
        char c = (char)key;
        if (key == KEYBOARD_KEY_NONE) continue;

        if (key == KEYBOARD_KEY_LEFT) {
            if (input_cursor > 0) {
                terminal_move_cursor_left();
                input_cursor--;
            }
        } else if (key == KEYBOARD_KEY_RIGHT) {
            if (input_cursor < input_length) {
                terminal_putchar(input[input_cursor]);
                input_cursor++;
            }
        } else if (c == '\b') {
            if (input_cursor > 0) {
                int index;

                terminal_backspace();
                for (index = input_cursor - 1; index < input_length - 1; index++)
                    input[index] = input[index + 1];
                input_length--;
                input_cursor--;

                for (index = input_cursor; index < input_length; index++)
                    terminal_putchar(input[index]);
                terminal_putchar(' ');
                for (index = input_cursor; index <= input_length; index++)
                    terminal_backspace();
            }
        } else if (c == '\n') {
            terminal_putchar('\n');
            input[input_length] = '\0';
            if (input_length != 0 && !commands_execute(input))
                terminal_writestring("Unknown command\n");
            terminal_writestring("> ");
            input_length = 0;
            input_cursor = 0;
        } else if (input_length < (int)(sizeof(input) - 1)) {
            int index;

            for (index = input_length; index > input_cursor; index--)
                input[index] = input[index - 1];
            input[input_cursor] = c;
            input_length++;
            terminal_putchar(c);
            for (index = input_cursor + 1; index < input_length; index++)
                terminal_putchar(input[index]);
            for (index = input_cursor + 1; index < input_length; index++)
                terminal_move_cursor_left();
            input_cursor++;
        }
    }
}

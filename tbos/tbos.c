#include "tbos.h"

#include "../drivers/keyboard.h"
#include "../filesystem/filesystem.h"
#include "../terminal/terminal.h"

#include <stddef.h>
#include <stdint.h>

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define STATUS_ROWS 2
#define EDIT_ROWS (VGA_HEIGHT - STATUS_ROWS)
#define BUFFER_CAPACITY FS_MAX_FILE_SIZE

static uint8_t buffer[BUFFER_CAPACITY];
static uint32_t buffer_size;
static char file_path[56];
static int cursor_row;
static int cursor_col;
static int scroll_offset;
static int modified;

extern size_t terminal_row;
extern size_t terminal_column;
extern uint8_t terminal_color;
extern volatile uint16_t *terminal_buffer;

static uint16_t vga_make(char c, uint8_t color) {
    return (uint16_t)(unsigned char)c | (uint16_t)color << 8;
}

static uint8_t make_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

static void copy_string(char *dest, const char *src, size_t capacity) {
    size_t index = 0;

    while (index + 1 < capacity && src[index] != '\0') {
        dest[index] = src[index];
        index++;
    }
    dest[index] = '\0';
}

static size_t string_len(const char *text) {
    size_t length = 0;

    while (text[length] != '\0')
        length++;
    return length;
}

static int line_count(void) {
    int lines = 1;
    uint32_t index;

    for (index = 0; index < buffer_size; index++) {
        if (buffer[index] == '\n')
            lines++;
    }
    return lines;
}

static uint32_t line_start(int line) {
    int current = 0;
    uint32_t index = 0;

    while (current < line && index < buffer_size) {
        if (buffer[index] == '\n')
            current++;
        index++;
    }
    return index;
}

static int line_length(int line) {
    uint32_t start = line_start(line);
    int length = 0;

    while (start + (uint32_t)length < buffer_size &&
           buffer[start + length] != '\n')
        length++;
    return length;
}

static void insert_char_at(uint32_t position, uint8_t c) {
    uint32_t index;

    if (buffer_size >= BUFFER_CAPACITY)
        return;
    for (index = buffer_size; index > position; index--)
        buffer[index] = buffer[index - 1];
    buffer[position] = c;
    buffer_size++;
    modified = 1;
}

static void delete_char_at(uint32_t position) {
    uint32_t index;

    if (position >= buffer_size)
        return;
    for (index = position; index + 1 < buffer_size; index++)
        buffer[index] = buffer[index + 1];
    buffer_size--;
    modified = 1;
}

static uint32_t cursor_position(void) {
    return line_start(cursor_row) + (uint32_t)cursor_col;
}

static void clamp_cursor_col(void) {
    int length = line_length(cursor_row);

    if (cursor_col > length)
        cursor_col = length;
}

static void ensure_visible(void) {
    if (cursor_row < scroll_offset)
        scroll_offset = cursor_row;
    if (cursor_row >= scroll_offset + EDIT_ROWS)
        scroll_offset = cursor_row - EDIT_ROWS + 1;
}

static void write_number(int x, int y, uint8_t color, int value) {
    char digits[6];
    int count = 0;
    int index;

    if (value == 0) {
        terminal_buffer[(size_t)y * VGA_WIDTH + (size_t)x] =
            vga_make('0', color);
        return;
    }
    while (value > 0 && count < 6) {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    }
    for (index = count - 1; index >= 0; index--) {
        if (x < VGA_WIDTH)
            terminal_buffer[(size_t)y * VGA_WIDTH + (size_t)x++] =
                vga_make(digits[index], color);
    }
}

static void draw_status_bar(void) {
    uint8_t bar_color = make_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
    uint8_t hint_color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    int x;
    int name_x;
    size_t path_len = string_len(file_path);
    const char *hint = "Ctrl+S Save | Ctrl+Q Quit";

    for (x = 0; x < VGA_WIDTH; x++)
        terminal_buffer[EDIT_ROWS * VGA_WIDTH + x] = vga_make(' ', bar_color);

    name_x = 1;
    for (x = 0; x < (int)path_len && name_x < VGA_WIDTH - 20; x++)
        terminal_buffer[EDIT_ROWS * VGA_WIDTH + name_x++] =
            vga_make(file_path[x], bar_color);

    if (modified) {
        if (name_x < VGA_WIDTH - 18) {
            terminal_buffer[EDIT_ROWS * VGA_WIDTH + name_x++] =
                vga_make(' ', bar_color);
            terminal_buffer[EDIT_ROWS * VGA_WIDTH + name_x++] =
                vga_make('[', bar_color);
            terminal_buffer[EDIT_ROWS * VGA_WIDTH + name_x++] =
                vga_make('+', bar_color);
            terminal_buffer[EDIT_ROWS * VGA_WIDTH + name_x++] =
                vga_make(']', bar_color);
        }
    }

    x = VGA_WIDTH - 12;
    terminal_buffer[EDIT_ROWS * VGA_WIDTH + x++] = vga_make('L', bar_color);
    write_number(x, EDIT_ROWS, bar_color, cursor_row + 1);
    x += 3;
    terminal_buffer[EDIT_ROWS * VGA_WIDTH + x++] = vga_make(' ', bar_color);
    terminal_buffer[EDIT_ROWS * VGA_WIDTH + x++] = vga_make('C', bar_color);
    write_number(x, EDIT_ROWS, bar_color, cursor_col + 1);

    for (x = 0; x < VGA_WIDTH; x++)
        terminal_buffer[(EDIT_ROWS + 1) * VGA_WIDTH + x] =
            vga_make(' ', hint_color);
    for (x = 0; hint[x] != '\0' && x + 1 < VGA_WIDTH; x++)
        terminal_buffer[(EDIT_ROWS + 1) * VGA_WIDTH + 1 + x] =
            vga_make(hint[x], hint_color);
}

static void draw_screen(void) {
    uint8_t text_color = make_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    uint8_t empty_color = make_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    int total_lines = line_count();
    int row;

    for (row = 0; row < EDIT_ROWS; row++) {
        int screen_line = scroll_offset + row;
        int col;

        if (screen_line < total_lines) {
            int length = line_length(screen_line);
            uint32_t start = line_start(screen_line);

            for (col = 0; col < VGA_WIDTH; col++) {
                if (col < length)
                    terminal_buffer[row * VGA_WIDTH + col] =
                        vga_make((char)buffer[start + col], text_color);
                else
                    terminal_buffer[row * VGA_WIDTH + col] =
                        vga_make(' ', text_color);
            }
        } else {
            terminal_buffer[row * VGA_WIDTH] = vga_make('~', empty_color);
            for (col = 1; col < VGA_WIDTH; col++)
                terminal_buffer[row * VGA_WIDTH + col] =
                    vga_make(' ', empty_color);
        }
    }
    draw_status_bar();
    update_cursor(cursor_col, cursor_row - scroll_offset);
}

static void show_message(const char *text) {
    uint8_t msg_color = make_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    int x;

    for (x = 0; x < VGA_WIDTH; x++)
        terminal_buffer[(EDIT_ROWS + 1) * VGA_WIDTH + x] =
            vga_make(' ', msg_color);
    for (x = 0; text[x] != '\0' && x + 1 < VGA_WIDTH; x++)
        terminal_buffer[(EDIT_ROWS + 1) * VGA_WIDTH + 1 + x] =
            vga_make(text[x], msg_color);
    update_cursor(cursor_col, cursor_row - scroll_offset);
}

static int do_save(void) {
    if (filesystem_save_file_data(file_path, buffer, buffer_size)) {
        modified = 0;
        return 1;
    }
    return 0;
}

static void handle_arrow_up(void) {
    if (cursor_row > 0) {
        cursor_row--;
        clamp_cursor_col();
        ensure_visible();
    }
}

static void handle_arrow_down(void) {
    if (cursor_row < line_count() - 1) {
        cursor_row++;
        clamp_cursor_col();
        ensure_visible();
    }
}

static void handle_arrow_left(void) {
    if (cursor_col > 0) {
        cursor_col--;
    } else if (cursor_row > 0) {
        cursor_row--;
        cursor_col = line_length(cursor_row);
        ensure_visible();
    }
}

static void handle_arrow_right(void) {
    int length = line_length(cursor_row);

    if (cursor_col < length) {
        cursor_col++;
    } else if (cursor_row < line_count() - 1) {
        cursor_row++;
        cursor_col = 0;
        ensure_visible();
    }
}

static void handle_home(void) {
    cursor_col = 0;
}

static void handle_end(void) {
    cursor_col = line_length(cursor_row);
}

static void handle_page_up(void) {
    int jump = EDIT_ROWS;

    cursor_row -= jump;
    if (cursor_row < 0)
        cursor_row = 0;
    scroll_offset -= jump;
    if (scroll_offset < 0)
        scroll_offset = 0;
    clamp_cursor_col();
}

static void handle_page_down(void) {
    int jump = EDIT_ROWS;
    int total = line_count();

    cursor_row += jump;
    if (cursor_row >= total)
        cursor_row = total - 1;
    scroll_offset += jump;
    if (scroll_offset > total - EDIT_ROWS)
        scroll_offset = total - EDIT_ROWS;
    if (scroll_offset < 0)
        scroll_offset = 0;
    clamp_cursor_col();
}

static void handle_enter(void) {
    uint32_t pos = cursor_position();

    insert_char_at(pos, '\n');
    cursor_row++;
    cursor_col = 0;
    ensure_visible();
}

static void handle_backspace(void) {
    uint32_t pos;

    if (cursor_col > 0) {
        pos = cursor_position();
        delete_char_at(pos - 1);
        cursor_col--;
    } else if (cursor_row > 0) {
        cursor_col = line_length(cursor_row - 1);
        pos = cursor_position();
        delete_char_at(pos - 1);
        cursor_row--;
        ensure_visible();
    }
}

static void handle_delete(void) {
    uint32_t pos = cursor_position();

    if (pos < buffer_size)
        delete_char_at(pos);
}

static void handle_printable(char c) {
    uint32_t pos = cursor_position();

    insert_char_at(pos, (uint8_t)c);
    cursor_col++;
}

static void restore_terminal(void) {
    int x, y;
    uint8_t color = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    for (y = 0; y < VGA_HEIGHT; y++)
        for (x = 0; x < VGA_WIDTH; x++)
            terminal_buffer[y * VGA_WIDTH + x] = vga_make(' ', color);
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = color;
    update_cursor(0, 0);
}

void tbos_open(const char *path) {
    uint32_t loaded_size = 0;
    int running = 1;

    copy_string(file_path, path, sizeof(file_path));
    buffer_size = 0;
    cursor_row = 0;
    cursor_col = 0;
    scroll_offset = 0;
    modified = 0;

    if (filesystem_file_exists(path)) {
        if (!filesystem_load_file_data(path, buffer, BUFFER_CAPACITY,
                                       &loaded_size)) {
            terminal_writestring("tbos: could not read file\n");
            return;
        }
        buffer_size = loaded_size;
    }

    draw_screen();

    while (running) {
        int key = keyboard_read_key();
        int ctrl = keyboard_ctrl_held();

        if (ctrl) {
            char c = (char)(key & 0xFF);

            if (c == 's' || c == 'S') {
                if (do_save())
                    show_message("Saved.");
                else
                    show_message("Save failed!");
                draw_status_bar();
                update_cursor(cursor_col, cursor_row - scroll_offset);
                continue;
            }
            if (c == 'q' || c == 'Q') {
                running = 0;
                continue;
            }
        }

        if (key == KEYBOARD_KEY_UP) {
            handle_arrow_up();
        } else if (key == KEYBOARD_KEY_DOWN) {
            handle_arrow_down();
        } else if (key == KEYBOARD_KEY_LEFT) {
            handle_arrow_left();
        } else if (key == KEYBOARD_KEY_RIGHT) {
            handle_arrow_right();
        } else if (key == KEYBOARD_KEY_HOME) {
            handle_home();
        } else if (key == KEYBOARD_KEY_END) {
            handle_end();
        } else if (key == KEYBOARD_KEY_PGUP) {
            handle_page_up();
        } else if (key == KEYBOARD_KEY_PGDN) {
            handle_page_down();
        } else if (key == KEYBOARD_KEY_DELETE) {
            handle_delete();
        } else if ((char)key == '\b') {
            handle_backspace();
        } else if ((char)key == '\n') {
            handle_enter();
        } else if (key >= 0x20 && key < 0x7F) {
            handle_printable((char)key);
        } else {
            continue;
        }
        draw_screen();
    }

    restore_terminal();
}

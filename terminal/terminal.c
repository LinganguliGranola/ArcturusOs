#include "terminal.h"
#include "../drivers/io.h"

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
	return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
	return (uint16_t) uc | (uint16_t) color << 8;
}

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

// Scroll-back buffer: SCROLLBACK_LINES total rows of history
#define SCROLLBACK_LINES 200

static uint16_t scrollback[SCROLLBACK_LINES][80];

// next_line is the index in scrollback where the next new line will be written (ring buffer)
static size_t next_line = 0;
// total_lines is how many lines have been written into scrollback (capped at SCROLLBACK_LINES)
static size_t total_lines = 0;
// view_offset: 0 = showing the bottom (live view); positive = scrolled up by that many lines
static size_t view_offset = 0;

size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
volatile uint16_t* terminal_buffer;

void enable_cursor(uint8_t cursor_start, uint8_t cursor_end) {
	outb(0x3D4, 0x0A);
	outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);

	outb(0x3D4, 0x0B);
	outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

void update_cursor(int x, int y) {
	uint16_t pos = y * VGA_WIDTH + x;
	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t) (pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
}

// Re-render VGA from scrollback according to view_offset
static void render_view(void) {
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		long idx = (long)next_line - (long)VGA_HEIGHT + (long)y - (long)view_offset;

		for (size_t x = 0; x < VGA_WIDTH; x++) {
			size_t vga_idx = y * VGA_WIDTH + x;
			if (idx < 0 || (size_t)idx >= total_lines) {
				terminal_buffer[vga_idx] = vga_entry(' ', vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
			} else {
				size_t sb_row = ((size_t)idx) % SCROLLBACK_LINES;
				terminal_buffer[vga_idx] = scrollback[sb_row][x];
			}
		}
	}
}

void terminal_initialize(void) {
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = (volatile uint16_t*) 0xB8000;

	// Initialize all scrollback lines to blank
	for (size_t r = 0; r < SCROLLBACK_LINES; r++)
		for (size_t x = 0; x < VGA_WIDTH; x++)
			scrollback[r][x] = vga_entry(' ', terminal_color);

	// Seed scrollback with VGA_HEIGHT empty lines so indexing works from the start
	next_line = VGA_HEIGHT;
	total_lines = VGA_HEIGHT;

	render_view();
    enable_cursor(0, 15);
    update_cursor(terminal_column, terminal_row);
}

void terminal_setcolor(uint8_t color) {
	terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
	const size_t index = y * VGA_WIDTH + x;
	uint16_t entry = vga_entry(c, color);
	terminal_buffer[index] = entry;
	// Mirror into scrollback so history stays correct
	size_t abs_row = (next_line >= VGA_HEIGHT ? next_line - VGA_HEIGHT : 0) + y;
	scrollback[abs_row % SCROLLBACK_LINES][x] = entry;
}

void terminal_backspace(void) {
	if (terminal_column == 0) {
		if (terminal_row > 0) {
			terminal_row--;
			terminal_column = 79;
		}
	} else {
		terminal_column--;
	}
	terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
	update_cursor(terminal_column, terminal_row);
}

void terminal_move_cursor_left(void) {
	if (terminal_column == 0) {
		if (terminal_row > 0) {
			terminal_row--;
			terminal_column = VGA_WIDTH - 1;
		}
	} else {
		terminal_column--;
	}
	update_cursor(terminal_column, terminal_row);
}

void terminal_move_cursor_right(void) {
	if (++terminal_column == VGA_WIDTH) {
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT)
			terminal_row = 0;
	}
	update_cursor(terminal_column, terminal_row);
}

static void terminal_scroll(void) {
	// Advance to a new scrollback line
	size_t new_row = next_line % SCROLLBACK_LINES;
	for (size_t x = 0; x < VGA_WIDTH; x++)
		scrollback[new_row][x] = vga_entry(' ', terminal_color);
	next_line++;
	if (total_lines < SCROLLBACK_LINES)
		total_lines++;

	// If user is not scrolled up, follow the new output
	if (view_offset == 0) {
		render_view();
	}
	// If user IS scrolled up, don't move their view; the new line goes into the buffer silently
}

void terminal_putchar(char c) {
	if (c == '\n') {
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT) {
			terminal_scroll();
			terminal_row = VGA_HEIGHT - 1;
		}
		update_cursor(terminal_column, terminal_row);
		return;
	}
	terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
	if (++terminal_column == VGA_WIDTH) {
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT) {
			terminal_scroll();
			terminal_row = VGA_HEIGHT - 1;
		}
	}
	update_cursor(terminal_column, terminal_row);
}

void terminal_write(const char* data, size_t size) {
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
	size_t datalen = 0;
	while (data[datalen])
		datalen++;
	terminal_write(data, datalen);
}

void terminal_scroll_view_up(void) {
	// Max we can scroll up: total_lines - VGA_HEIGHT
	size_t max_offset = total_lines > VGA_HEIGHT ? total_lines - VGA_HEIGHT : 0;
	if (view_offset < max_offset) {
		view_offset++;
		render_view();
		// Hide cursor when scrolled up
		update_cursor(0, VGA_HEIGHT);
	}
}

void terminal_scroll_view_down(void) {
	if (view_offset > 0) {
		view_offset--;
		render_view();
		if (view_offset == 0)
			update_cursor(terminal_column, terminal_row);
		else
			update_cursor(0, VGA_HEIGHT);
	}
}

int terminal_is_scrolled(void) {
	return view_offset > 0;
}

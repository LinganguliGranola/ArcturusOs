#include "keyboard.h"
#include "io.h"
#include <stdint.h>

static const char scancode_map[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, // Ctrl
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, // LShift
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',
    0, // RShift
    '*',
    0, // LAlt
    ' ', // Space
    0, // CapsLock
    0, // F1 
    0, 0, 0, 0, 0, 0, 0, 0, 0, // F2-F10
    0, // NumLock
    0, // ScrollLock
    0, // Home
    0, // Up Arrow
    0, // Page Up
    '-',
    0, // Left Arrow
    0,
    0, // Right Arrow
    '+',
    0, // End
    0, // Down Arrow
    0, // Page Down
    0, // Insert
    0  // Delete
};

static const char shift_scancode_map[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*',
    '(', ')', '_', '+', '\b', '\t', 'Q', 'W', 'E', 'R',
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0, '|', 'Z', 'X', 'C', 'V', 'B', 'N',
    'M', '<', '>', '?', 0, '*', 0, ' ', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, '-', 0, 0, 0, '+', 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

void keyboard_init(void) {
    // In polling mode, usually no complex init is strictly necessary for PS/2,
    // assuming the BIOS/bootloader left it ready for read.
}

static int lshift_down = 0;
static int rshift_down = 0;
static int caps_lock = 0;

int keyboard_read_key(void) {
    uint8_t extended = 0;

    while (1) {
        // Check if data is available on status port 0x64
        if (inb(0x64) & 1) {
            uint8_t scancode = inb(0x60);
            
            if (scancode == 0xE0) {
                extended = 1;
                continue;
            }

            if (extended) {
                extended = 0;
                if (scancode & 0x80)
                    continue;
                if (scancode == 0x4B)
                    return KEYBOARD_KEY_LEFT;
                if (scancode == 0x4D)
                    return KEYBOARD_KEY_RIGHT;
                continue;
            }

            // If the highest bit is set, it's a key release (break code)
            if (scancode & 0x80) {
                uint8_t released_code = scancode & ~0x80;
                if (released_code == 0x2A) {
                    lshift_down = 0;
                } else if (released_code == 0x36) {
                    rshift_down = 0;
                }
                continue;
            }
            
            // Key press
            if (scancode == 0x2A) {
                lshift_down = 1;
                continue;
            } else if (scancode == 0x36) {
                rshift_down = 1;
                continue;
            } else if (scancode == 0x3A) {
                caps_lock ^= 1;
                continue;
            }
            
            if (scancode < 128) {
                char c = scancode_map[scancode];
                int is_shift = lshift_down || rshift_down;
                
                // Alphabetic characters logic
                if (c >= 'a' && c <= 'z') {
                    if (is_shift ^ caps_lock) {
                        return shift_scancode_map[scancode];
                    } else {
                        return scancode_map[scancode];
                    }
                } 
                // Other characters
                else {
                    if (is_shift) {
                        return shift_scancode_map[scancode];
                    } else {
                        return scancode_map[scancode];
                    }
                }
            }
        }
    }
}

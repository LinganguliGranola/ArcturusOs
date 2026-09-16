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

void keyboard_init(void) {
    // In polling mode, usually no complex init is strictly necessary for PS/2,
    // assuming the BIOS/bootloader left it ready for read.
}

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

            // If the highest bit is set, it's a key release (break code), ignore it
            if (scancode & 0x80) {
                continue;
            }
            
            if (scancode < 128) {
                return scancode_map[scancode];
            }
        }
    }
}

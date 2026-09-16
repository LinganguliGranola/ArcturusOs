#ifndef KEYBOARD_H
#define KEYBOARD_H

enum keyboard_key {
    KEYBOARD_KEY_NONE = 0,
    KEYBOARD_KEY_LEFT = 0x100,
    KEYBOARD_KEY_RIGHT,
};

void keyboard_init(void);
int keyboard_read_key(void);

#endif

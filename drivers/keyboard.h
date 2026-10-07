#ifndef KEYBOARD_H
#define KEYBOARD_H

enum keyboard_key {
    KEYBOARD_KEY_NONE = 0,
    KEYBOARD_KEY_LEFT = 0x100,
    KEYBOARD_KEY_RIGHT,
    KEYBOARD_KEY_UP,
    KEYBOARD_KEY_DOWN,
    KEYBOARD_KEY_HOME,
    KEYBOARD_KEY_END,
    KEYBOARD_KEY_PGUP,
    KEYBOARD_KEY_PGDN,
    KEYBOARD_KEY_DELETE,
};

void keyboard_init(void);
int keyboard_read_key(void);
int keyboard_ctrl_held(void);

#endif

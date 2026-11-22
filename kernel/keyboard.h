#ifndef ASTEROS_KEYBOARD_H
#define ASTEROS_KEYBOARD_H

#include <stdint.h>

/* Non-ASCII input events passed through gui_handle_key(). */
enum {
    ASTEROS_KEY_UP = 0x80,
    ASTEROS_KEY_DOWN = 0x81,
    ASTEROS_KEY_HOVER_LEFT_CLICK = 0x82,
    ASTEROS_KEY_HOVER_RIGHT_CLICK = 0x83,
    ASTEROS_KEY_CLOSE_APP = 0x84,
    ASTEROS_KEY_LEFT = 0x85,
    ASTEROS_KEY_RIGHT = 0x86,
    ASTEROS_KEY_HOME = 0x87,
    ASTEROS_KEY_END = 0x88,
    ASTEROS_KEY_DELETE = 0x89,
    ASTEROS_KEY_SAVE = 0x8A,
    ASTEROS_KEY_SAVE_AS = 0x8B,
    ASTEROS_KEY_OPEN = 0x8C,
    ASTEROS_KEY_CHECK = 0x8D
};

/* Decode one byte from the PS/2 Set 1 scancode stream.
   Returns 1 when a printable/control/navigation key is produced. */
uint8_t keyboard_decode_scancode(uint8_t scancode, char *key);

/* Clear modifier and prefix state for deterministic startup and tests. */
void keyboard_reset_state(void);

#endif

#ifndef ASTEROS_KEYBOARD_H
#define ASTEROS_KEYBOARD_H

#include <stdint.h>

enum { ASTEROS_KEY_UP = 0x80, ASTEROS_KEY_DOWN = 0x81 };

void keyboard_reset_state(void);
uint8_t keyboard_decode_scancode(uint8_t scancode, char *key);

#endif

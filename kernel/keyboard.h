#ifndef ASTEROS_KEYBOARD_H
#define ASTEROS_KEYBOARD_H

#include <stdint.h>

/* Decode one byte from the PS/2 Set 1 keyboard stream.
   Returns 1 when a character/navigation key was produced, otherwise 0. */
uint8_t keyboard_decode(uint8_t scancode, char *key);

/* Reset modifier and prefix state; primarily useful for deterministic tests. */
void keyboard_reset(void);

#endif

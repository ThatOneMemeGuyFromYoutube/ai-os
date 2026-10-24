#ifndef ASTEROS_DESKTOP_INPUT_H
#define ASTEROS_DESKTOP_INPUT_H

#include <stdint.h>

enum {
    ASTEROS_APP_TERMINAL = 0,
    ASTEROS_APP_FILES = 1,
    ASTEROS_APP_PROGRAMS = 2,
    ASTEROS_APP_ABOUT = 3,
    ASTEROS_APP_SETTINGS = 4
};

enum {
    ASTEROS_HIT_START = -1,
    ASTEROS_HIT_CLOSE = -2,
    ASTEROS_HIT_NONE = -3,
    ASTEROS_HIT_SETTING_VISIBLE = -4,
    ASTEROS_HIT_SETTING_SPEED = -5,
    ASTEROS_HIT_SETTING_SHAPE = -6,
    ASTEROS_HIT_SETTING_COLOR = -7,
    ASTEROS_HIT_SETTING_KEYBOARD_CLICK = -8
};

/* Translate VGA text-cell coordinates into desktop controls or app IDs.
   Taskbar controls stay available while a window is open. Window contents
   take precedence over desktop icons behind them. */
int8_t asteros_desktop_hit_test(int8_t active_app, uint8_t x, uint8_t y);

/* Left, right, and middle presses can activate controls. */
uint8_t asteros_mouse_activation_edge(uint8_t previous_buttons, uint8_t buttons);

#endif

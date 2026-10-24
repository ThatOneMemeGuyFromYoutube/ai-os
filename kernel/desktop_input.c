#include "desktop_input.h"

uint8_t asteros_mouse_activation_edge(uint8_t previous_buttons, uint8_t buttons) {
    const uint8_t activation_buttons = 0x07;
    return (uint8_t)((buttons & activation_buttons) &
                     (uint8_t)~(previous_buttons & activation_buttons));
}

int8_t asteros_desktop_hit_test(int8_t active_app, uint8_t x, uint8_t y) {
    /* The taskbar is always in front of the desktop and active window. */
    if (y == 23) {
        if (x >= 1 && x <= 9) return ASTEROS_HIT_START;
        if (x >= 12 && x <= 19) return ASTEROS_APP_TERMINAL;
        if (x >= 22 && x <= 27) return ASTEROS_APP_FILES;
        if (x >= 29 && x <= 37) return ASTEROS_APP_PROGRAMS;
        if (x >= 40 && x <= 44) return ASTEROS_APP_ABOUT;
        if (x >= 47 && x <= 54) return ASTEROS_APP_SETTINGS;
        return ASTEROS_HIT_NONE;
    }

    if (active_app >= 0) {
        if (y == 4 && x >= 72 && x <= 76) return ASTEROS_HIT_CLOSE;
        if (active_app == ASTEROS_APP_SETTINGS && x >= 22 && x <= 66) {
            if (y >= 7 && y <= 8) return ASTEROS_HIT_SETTING_VISIBLE;
            if (y >= 9 && y <= 10) return ASTEROS_HIT_SETTING_SPEED;
            if (y >= 11 && y <= 12) return ASTEROS_HIT_SETTING_SHAPE;
            if (y >= 13 && y <= 14) return ASTEROS_HIT_SETTING_COLOR;
            if (y == 15) return ASTEROS_HIT_SETTING_KEYBOARD_CLICK;
        }
        /* Do not activate desktop icons through the foreground window. */
        return ASTEROS_HIT_NONE;
    }

    /* The launcher lists all five applications, including Settings. */
    if (x >= 2 && x <= 17) {
        if (y >= 5 && y <= 6) return ASTEROS_APP_TERMINAL;
        if (y >= 7 && y <= 8) return ASTEROS_APP_FILES;
        if (y >= 9 && y <= 10) return ASTEROS_APP_PROGRAMS;
        if (y >= 11 && y <= 12) return ASTEROS_APP_ABOUT;
        if (y >= 13 && y <= 14) return ASTEROS_APP_SETTINGS;
    }

    /* Desktop icons are interactive only when no application window covers them. */
    if (x >= 23 && x <= 30 && y >= 7 && y <= 9) return ASTEROS_APP_TERMINAL;
    if (x >= 33 && x <= 40 && y >= 7 && y <= 9) return ASTEROS_APP_FILES;
    if (x >= 43 && x <= 50 && y >= 7 && y <= 9) return ASTEROS_APP_PROGRAMS;
    if (x >= 53 && x <= 60 && y >= 7 && y <= 9) return ASTEROS_APP_FILES;

    return ASTEROS_HIT_NONE;
}

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
    ASTEROS_CONTEXT_MENU_WIDTH = 16,
    ASTEROS_CONTEXT_MENU_HEIGHT = 5,
    ASTEROS_CONTEXT_MENU_NONE = 0,
    ASTEROS_CONTEXT_MENU_OPEN = 1,
    ASTEROS_CONTEXT_MENU_SETTINGS = 2,
    ASTEROS_CONTEXT_MENU_CLOSE = 3
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

/* Resolve the object beneath a context-menu request. A click inside the
   foreground window targets that window even where ordinary activation hit
   testing intentionally blocks its contents. */
int8_t asteros_context_menu_resolve_target(int8_t active_app, uint8_t x,
                                           uint8_t y, int8_t hit_target);

/* Return nonzero only when a valid application request changes focus. */
uint8_t asteros_app_focus_changed(int8_t active_app, int8_t requested_app);

/* A minimized app restores its previous output; other focus changes reset it. */
uint8_t asteros_app_activation_resets_output(int8_t active_app,
                                             int8_t requested_app,
                                             int8_t minimized_app);

/* Return nonzero when clicking the active app's taskbar button should minimize it.
   The single foreground window is toggled only by its own taskbar button. */
uint8_t asteros_taskbar_should_minimize(int8_t active_app, int8_t requested_app,
                                        uint8_t y);

/* Place a 16x5 context menu wholly inside the 80x25 text screen. */
void asteros_context_menu_place(uint8_t pointer_x, uint8_t pointer_y,
                                uint8_t *menu_x, uint8_t *menu_y);

/* Return OPEN, SETTINGS, CLOSE, or NONE. Border cells are not actionable. */
int8_t asteros_context_menu_hit_test(uint8_t menu_x, uint8_t menu_y,
                                     uint8_t x, uint8_t y);

/* Move through the three actionable menu rows, wrapping at either end. */
uint8_t asteros_context_menu_move_selection(uint8_t selection, int8_t delta);

/* Left, right, and middle presses can activate controls. */
uint8_t asteros_mouse_activation_edge(uint8_t previous_buttons, uint8_t buttons);

#endif

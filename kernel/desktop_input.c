#include "desktop_input.h"

uint8_t asteros_mouse_click_filter_update(asteros_mouse_click_filter_t *filter,
                                           uint8_t left_down, uint32_t now) {
    uint8_t emit = 0;
    uint8_t rising_edge;

    if (!filter) return 0;

    /* Unsigned subtraction makes the deadline check safe across tick wrap. */
    if (filter->pending &&
        (uint32_t)(now - filter->deadline) < 0x80000000u) {
        filter->pending = 0;
        emit = 1;
    }

    rising_edge = (uint8_t)(left_down && !filter->previous_left);
    filter->previous_left = (uint8_t)(left_down != 0);
    if (rising_edge && !filter->pending) {
        filter->pending = 1;
        filter->deadline = now + ASTEROS_MOUSE_DOUBLE_TAP_WINDOW_TICKS;
    }

    return emit;
}

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
        if (x >= 56 && x <= 60) return ASTEROS_APP_IDE;
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

    /* The launcher lists all six applications, including Settings and IDE. */
    if (x >= 2 && x <= 17) {
        if (y >= 5 && y <= 6) return ASTEROS_APP_TERMINAL;
        if (y >= 7 && y <= 8) return ASTEROS_APP_FILES;
        if (y >= 9 && y <= 10) return ASTEROS_APP_PROGRAMS;
        if (y >= 11 && y <= 12) return ASTEROS_APP_ABOUT;
        if (y >= 13 && y <= 14) return ASTEROS_APP_SETTINGS;
        if (y >= 15 && y <= 16) return ASTEROS_APP_IDE;
    }

    /* Desktop icons are interactive only when no application window covers them. */
    if (x >= 23 && x <= 30 && y >= 7 && y <= 9) return ASTEROS_APP_TERMINAL;
    if (x >= 33 && x <= 40 && y >= 7 && y <= 9) return ASTEROS_APP_FILES;
    if (x >= 43 && x <= 50 && y >= 7 && y <= 9) return ASTEROS_APP_PROGRAMS;
    if (x >= 53 && x <= 60 && y >= 7 && y <= 9) return ASTEROS_APP_FILES;
    if (x >= 63 && x <= 70 && y >= 7 && y <= 9) return ASTEROS_APP_IDE;

    return ASTEROS_HIT_NONE;
}

int8_t asteros_context_menu_resolve_target(int8_t active_app, uint8_t x,
                                           uint8_t y, int8_t hit_target) {
    /* gui_draw() places the active window at (20,2) with size 59x19.
       Its interior and title bar belong to the foreground app for context
       actions, even though normal hit testing blocks clicks through it. */
    if (active_app >= ASTEROS_APP_TERMINAL && active_app < ASTEROS_APP_COUNT &&
        x >= 20u && x <= 78u && y >= 2u && y <= 20u)
        return active_app;
    return hit_target;
}

uint8_t asteros_app_close_discards_terminal_state(int8_t active_app) {
    return (uint8_t)(active_app == ASTEROS_APP_TERMINAL);
}

uint8_t asteros_taskbar_should_minimize(int8_t active_app, int8_t requested_app,
                                        uint8_t y) {
    return (uint8_t)(y == 23u &&
                     active_app >= ASTEROS_APP_TERMINAL &&
                     active_app < ASTEROS_APP_COUNT &&
                     requested_app == active_app);
}

uint8_t asteros_context_menu_close_targets_active_window(int8_t active_app,
                                                                      int8_t context_target) {
    return (uint8_t)(active_app >= ASTEROS_APP_TERMINAL &&
                     active_app < ASTEROS_APP_COUNT &&
                     context_target == active_app);
}

void asteros_context_menu_place(uint8_t pointer_x, uint8_t pointer_y,
                                uint8_t *menu_x, uint8_t *menu_y) {
    if (!menu_x || !menu_y) return;

    *menu_x = pointer_x;
    *menu_y = pointer_y;
    if (*menu_x > 80u - ASTEROS_CONTEXT_MENU_WIDTH)
        *menu_x = 80u - ASTEROS_CONTEXT_MENU_WIDTH;
    if (*menu_y > 25u - ASTEROS_CONTEXT_MENU_HEIGHT)
        *menu_y = 25u - ASTEROS_CONTEXT_MENU_HEIGHT;
}

int8_t asteros_context_menu_hit_test(uint8_t menu_x, uint8_t menu_y,
                                     uint8_t x, uint8_t y) {
    if (x <= menu_x || x >= menu_x + ASTEROS_CONTEXT_MENU_WIDTH - 1u ||
        y <= menu_y || y >= menu_y + ASTEROS_CONTEXT_MENU_HEIGHT - 1u)
        return ASTEROS_CONTEXT_MENU_NONE;

    return (int8_t)(y - menu_y);
}

uint8_t asteros_context_menu_move_selection(uint8_t selection, int8_t delta) {
    if (selection < ASTEROS_CONTEXT_MENU_OPEN ||
        selection > ASTEROS_CONTEXT_MENU_CLOSE)
        selection = ASTEROS_CONTEXT_MENU_OPEN;

    if (delta < 0) {
        return selection == ASTEROS_CONTEXT_MENU_OPEN
            ? ASTEROS_CONTEXT_MENU_CLOSE : (uint8_t)(selection - 1u);
    }
    if (delta > 0) {
        return selection == ASTEROS_CONTEXT_MENU_CLOSE
            ? ASTEROS_CONTEXT_MENU_OPEN : (uint8_t)(selection + 1u);
    }
    return selection;
}

static char ascii_lower(char value) {
    if (value >= 'A' && value <= 'Z')
        return (char)(value - ('A' - 'a'));
    return value;
}

int8_t asteros_start_menu_hit_test(uint8_t x, uint8_t y) {
    uint8_t first_item_y = ASTEROS_START_MENU_Y + 2u;
    uint8_t last_item_y = (uint8_t)(first_item_y +
                                    ASTEROS_START_MENU_ITEM_COUNT - 1u);
    if (x <= ASTEROS_START_MENU_X ||
        x >= ASTEROS_START_MENU_X + ASTEROS_START_MENU_WIDTH - 1u ||
        y < first_item_y || y > last_item_y)
        return ASTEROS_HIT_NONE;
    return (int8_t)(y - first_item_y);
}

uint8_t asteros_start_menu_move_selection(uint8_t selection, int8_t delta) {
    if (selection >= ASTEROS_START_MENU_ITEM_COUNT) selection = 0u;
    if (delta < 0) {
        return selection == 0u
            ? (uint8_t)(ASTEROS_START_MENU_ITEM_COUNT - 1u)
            : (uint8_t)(selection - 1u);
    }
    if (delta > 0) {
        return selection + 1u >= ASTEROS_START_MENU_ITEM_COUNT
            ? 0u : (uint8_t)(selection + 1u);
    }
    return selection;
}

static uint8_t command_equals(const char *command, const char *expected) {
    while (*command == ' ' || *command == '\\t') command++;
    while (*expected) {
        if (!*command || ascii_lower(*command) != *expected) return 0u;
        command++;
        expected++;
    }
    while (*command == ' ' || *command == '\\t') command++;
    return (uint8_t)(*command == '\\0');
}

int8_t asteros_app_resolve_command(const char *command) {
    if (!command) return ASTEROS_HIT_NONE;
    if (command_equals(command, "terminal") ||
        command_equals(command, "term") ||
        command_equals(command, "console"))
        return ASTEROS_APP_TERMINAL;
    if (command_equals(command, "files") ||
        command_equals(command, "file") ||
        command_equals(command, "computer") ||
        command_equals(command, "explorer"))
        return ASTEROS_APP_FILES;
    if (command_equals(command, "programs") ||
        command_equals(command, "apps"))
        return ASTEROS_APP_PROGRAMS;
    if (command_equals(command, "ide") ||
        command_equals(command, "editor"))
        return ASTEROS_APP_IDE;
    if (command_equals(command, "settings") ||
        command_equals(command, "control panel"))
        return ASTEROS_APP_SETTINGS;
    if (command_equals(command, "about") ||
        command_equals(command, "help"))
        return ASTEROS_APP_ABOUT;
    return ASTEROS_HIT_NONE;
}

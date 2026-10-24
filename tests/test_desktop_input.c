#include <assert.h>
#include <stdio.h>
#include "desktop_input.h"

static void expect_hit(int8_t active_app, uint8_t x, uint8_t y, int8_t expected) {
    assert(asteros_desktop_hit_test(active_app, x, y) == expected);
}

static void test_alternate_mouse_activation(void) {
    assert(asteros_mouse_activation_edge(0, 1) == 1); /* left */
    assert(asteros_mouse_activation_edge(0, 2) == 2); /* right */
    assert(asteros_mouse_activation_edge(0, 4) == 4); /* middle */
    assert(asteros_mouse_activation_edge(1, 3) == 2); /* second button */
    assert(asteros_mouse_activation_edge(2, 2) == 0); /* held */
    assert(asteros_mouse_activation_edge(2, 0) == 0); /* release */
    assert(asteros_mouse_activation_edge(0, 8) == 0); /* unrelated bit */
}

static void test_taskbar_has_priority(void) {
    const int8_t active_apps[] = {-1, ASTEROS_APP_TERMINAL, ASTEROS_APP_SETTINGS};
    size_t i;

    for (i = 0; i < sizeof active_apps / sizeof active_apps[0]; i++) {
        int8_t active_app = active_apps[i];
        expect_hit(active_app, 1, 23, ASTEROS_HIT_START);
        expect_hit(active_app, 12, 23, ASTEROS_APP_TERMINAL);
        expect_hit(active_app, 22, 23, ASTEROS_APP_FILES);
        expect_hit(active_app, 29, 23, ASTEROS_APP_PROGRAMS);
        expect_hit(active_app, 40, 23, ASTEROS_APP_ABOUT);
        expect_hit(active_app, 47, 23, ASTEROS_APP_SETTINGS);
        expect_hit(active_app, 65, 23, ASTEROS_HIT_NONE);
    }
}

static void test_launcher_and_desktop_icons(void) {
    expect_hit(-1, 3, 5, ASTEROS_APP_TERMINAL);
    expect_hit(-1, 3, 7, ASTEROS_APP_FILES);
    expect_hit(-1, 3, 9, ASTEROS_APP_PROGRAMS);
    expect_hit(-1, 3, 11, ASTEROS_APP_ABOUT);
    expect_hit(-1, 3, 13, ASTEROS_APP_SETTINGS);

    expect_hit(-1, 25, 8, ASTEROS_APP_TERMINAL);
    expect_hit(-1, 35, 8, ASTEROS_APP_FILES);
    expect_hit(-1, 45, 8, ASTEROS_APP_PROGRAMS);
    expect_hit(-1, 55, 8, ASTEROS_APP_FILES);
    expect_hit(-1, 65, 8, ASTEROS_HIT_NONE);
    expect_hit(-1, 18, 13, ASTEROS_HIT_NONE);
}

static void test_foreground_window_blocks_background(void) {
    expect_hit(ASTEROS_APP_TERMINAL, 73, 4, ASTEROS_HIT_CLOSE);
    expect_hit(ASTEROS_APP_TERMINAL, 25, 8, ASTEROS_HIT_NONE);
    expect_hit(ASTEROS_APP_TERMINAL, 35, 8, ASTEROS_HIT_NONE);
    expect_hit(ASTEROS_APP_TERMINAL, 3, 13, ASTEROS_HIT_NONE);
    expect_hit(ASTEROS_APP_TERMINAL, 40, 7, ASTEROS_HIT_NONE);
    expect_hit(ASTEROS_APP_TERMINAL, 48, 23, ASTEROS_APP_SETTINGS);
    expect_hit(ASTEROS_APP_TERMINAL, 1, 23, ASTEROS_HIT_START);
}

static void test_settings_rows_are_scoped_and_complete(void) {
    expect_hit(ASTEROS_APP_SETTINGS, 22, 7, ASTEROS_HIT_SETTING_VISIBLE);
    expect_hit(ASTEROS_APP_SETTINGS, 66, 8, ASTEROS_HIT_SETTING_VISIBLE);
    expect_hit(ASTEROS_APP_SETTINGS, 22, 9, ASTEROS_HIT_SETTING_SPEED);
    expect_hit(ASTEROS_APP_SETTINGS, 66, 10, ASTEROS_HIT_SETTING_SPEED);
    expect_hit(ASTEROS_APP_SETTINGS, 22, 11, ASTEROS_HIT_SETTING_SHAPE);
    expect_hit(ASTEROS_APP_SETTINGS, 66, 12, ASTEROS_HIT_SETTING_SHAPE);
    expect_hit(ASTEROS_APP_SETTINGS, 22, 13, ASTEROS_HIT_SETTING_COLOR);
    expect_hit(ASTEROS_APP_SETTINGS, 66, 14, ASTEROS_HIT_SETTING_COLOR);
    expect_hit(ASTEROS_APP_SETTINGS, 22, 15, ASTEROS_HIT_SETTING_KEYBOARD_CLICK);
    expect_hit(ASTEROS_APP_SETTINGS, 66, 15, ASTEROS_HIT_SETTING_KEYBOARD_CLICK);
    expect_hit(ASTEROS_APP_SETTINGS, 21, 7, ASTEROS_HIT_NONE);
    expect_hit(ASTEROS_APP_SETTINGS, 67, 7, ASTEROS_HIT_NONE);
    expect_hit(ASTEROS_APP_SETTINGS, 21, 15, ASTEROS_HIT_NONE);
    expect_hit(ASTEROS_APP_SETTINGS, 67, 15, ASTEROS_HIT_NONE);
    expect_hit(ASTEROS_APP_SETTINGS, 40, 16, ASTEROS_HIT_NONE);
    expect_hit(ASTEROS_APP_SETTINGS, 73, 4, ASTEROS_HIT_CLOSE);
}

int main(void) {
    test_alternate_mouse_activation();
    test_taskbar_has_priority();
    test_launcher_and_desktop_icons();
    test_foreground_window_blocks_background();
    test_settings_rows_are_scoped_and_complete();
    puts("desktop input: tests passed");
    return 0;
}

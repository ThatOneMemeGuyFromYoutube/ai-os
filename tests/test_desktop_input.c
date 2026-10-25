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

static void test_context_menu_placement(void) {
    uint8_t x = 0, y = 0;

    asteros_context_menu_place(10, 7, &x, &y);
    assert(x == 10 && y == 7);

    asteros_context_menu_place(79, 24, &x, &y);
    assert(x == 64 && y == 20);
    assert(x + ASTEROS_CONTEXT_MENU_WIDTH == 80);
    assert(y + ASTEROS_CONTEXT_MENU_HEIGHT == 25);

    asteros_context_menu_place(64, 20, &x, &y);
    assert(x == 64 && y == 20);
    asteros_context_menu_place(0, 0, 0, &y);
    asteros_context_menu_place(0, 0, &x, 0);
}

static void test_context_menu_hit_testing(void) {
    const uint8_t left = 10, top = 5;

    assert(asteros_context_menu_hit_test(left, top, 12, 6) == ASTEROS_CONTEXT_MENU_OPEN);
    assert(asteros_context_menu_hit_test(left, top, 11, 7) == ASTEROS_CONTEXT_MENU_SETTINGS);
    assert(asteros_context_menu_hit_test(left, top, 24, 8) == ASTEROS_CONTEXT_MENU_CLOSE);

    /* No action is triggered by the border or a cell outside the menu. */
    assert(asteros_context_menu_hit_test(left, top, 10, 6) == ASTEROS_CONTEXT_MENU_NONE);
    assert(asteros_context_menu_hit_test(left, top, 25, 6) == ASTEROS_CONTEXT_MENU_NONE);
    assert(asteros_context_menu_hit_test(left, top, 12, 5) == ASTEROS_CONTEXT_MENU_NONE);
    assert(asteros_context_menu_hit_test(left, top, 12, 9) == ASTEROS_CONTEXT_MENU_NONE);
    assert(asteros_context_menu_hit_test(left, top, 9, 6) == ASTEROS_CONTEXT_MENU_NONE);
    assert(asteros_context_menu_hit_test(left, top, 26, 6) == ASTEROS_CONTEXT_MENU_NONE);
    assert(asteros_context_menu_hit_test(left, top, 12, 10) == ASTEROS_CONTEXT_MENU_NONE);

    /* A bottom-right menu remains selectable at the screen edge. */
    assert(asteros_context_menu_hit_test(64, 20, 66, 21) == ASTEROS_CONTEXT_MENU_OPEN);
    assert(asteros_context_menu_hit_test(64, 20, 66, 23) == ASTEROS_CONTEXT_MENU_CLOSE);
}

int main(void) {
    test_alternate_mouse_activation();
    test_taskbar_has_priority();
    test_launcher_and_desktop_icons();
    test_foreground_window_blocks_background();
    test_settings_rows_are_scoped_and_complete();
    test_context_menu_placement();
    test_context_menu_hit_testing();
    puts("desktop input: tests passed");
    return 0;
}

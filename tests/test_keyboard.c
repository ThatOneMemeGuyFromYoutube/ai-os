#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "keyboard.h"

static char decode(uint8_t scancode, int expected_event) {
    char key = 0;
    uint8_t produced = keyboard_decode_scancode(scancode, &key);
    assert(produced == (uint8_t)expected_event);
    return key;
}

static void expect_key(uint8_t scancode, char expected) {
    assert(decode(scancode, 1) == expected);
}

static void expect_ignored(uint8_t scancode) {
    char key = (char)0x55;
    assert(keyboard_decode_scancode(scancode, &key) == 0);
    assert(key == (char)0x55);
}

int main(void) {
    keyboard_reset_state();

    expect_key(0x1E, 'a');
    expect_ignored(0x9E); /* Key release is never text. */
    expect_key(0x30, 'b');

    expect_key(0x2A, (char)ASTEROS_KEY_HOVER_LEFT_CLICK); /* Left Shift down. */
    expect_ignored(0x2A); /* Key repeat must not activate again while held. */
    expect_key(0x1E, 'A');
    expect_key(0x02, '!');
    expect_key(0x1A, '{');
    expect_key(0x28, '"');
    expect_ignored(0xAA); /* Left Shift up. */
    expect_key(0x1E, 'a');
    expect_key(0x02, '1');

    expect_key(0x2A, (char)ASTEROS_KEY_HOVER_LEFT_CLICK);
    expect_key(0x36, (char)ASTEROS_KEY_HOVER_LEFT_CLICK); /* Both Shift keys may be held at once. */
    expect_ignored(0xAA); /* Releasing left Shift must preserve right Shift. */
    expect_key(0x1E, 'A');
    expect_ignored(0xB6);
    expect_key(0x1E, 'a');

    expect_ignored(0x3A); /* Caps Lock toggles only on the down edge. */
    expect_ignored(0x3A); /* A repeated make must not toggle it twice. */
    expect_key(0x1E, 'A');
    expect_ignored(0xBA);
    expect_key(0x2A, (char)ASTEROS_KEY_HOVER_LEFT_CLICK);
    expect_key(0x1E, 'a'); /* Shift XOR Caps Lock. */
    expect_ignored(0xAA);
    expect_ignored(0x3A); /* Caps Lock off. */
    expect_ignored(0xBA);
    expect_key(0x1E, 'a');

    expect_key(0x01, 27);
    expect_key(0x0F, '\t');
    expect_key(0x1C, '\r');
    expect_key(0x0E, '\b');
    expect_key(0x39, ' ');

    expect_ignored(0xE0);
    expect_key(0x48, (char)ASTEROS_KEY_UP);
    expect_ignored(0xE0);
    expect_ignored(0xC8); /* Extended key release. */
    expect_ignored(0xE0);
    expect_key(0x50, (char)ASTEROS_KEY_DOWN);
    expect_ignored(0xE0);
    expect_ignored(0x4B); /* Left arrow is not part of the GUI key ABI yet. */
    expect_key(0x1E, 'a'); /* Ignored extended keys cannot poison input. */

    expect_ignored(0xE0);
    expect_ignored(0x1D); /* Right Ctrl down. */
    expect_key(0x2A, (char)ASTEROS_KEY_HOVER_RIGHT_CLICK);
    expect_ignored(0xAA);
    expect_ignored(0xE0);
    expect_ignored(0x9D); /* Right Ctrl up. */

    /* Alt+F4 closes the foreground window with either Alt key. */
    expect_ignored(0x3E); /* F4 alone is not a desktop shortcut. */
    expect_ignored(0x38); /* Left Alt down. */
    expect_key(0x3E, (char)ASTEROS_KEY_CLOSE_APP);
    expect_ignored(0xB8); /* Left Alt up. */
    expect_ignored(0x3E);

    expect_ignored(0xE0);
    expect_ignored(0x38); /* Right Alt down. */
    expect_key(0x3E, (char)ASTEROS_KEY_CLOSE_APP);
    expect_ignored(0xE0);
    expect_ignored(0xB8); /* Right Alt up. */
    expect_ignored(0x3E);

    expect_ignored(0x38);
    keyboard_reset_state(); /* Reset must clear a held modifier. */
    expect_ignored(0x3E);

    expect_ignored(0xE1); /* Pause/Break prefix. */
    expect_ignored(0x1D);
    expect_ignored(0x45);
    expect_ignored(0xE1);
    expect_ignored(0x9D);
    expect_ignored(0xC5);
    expect_key(0x1E, 'a');

    assert(keyboard_decode_scancode(0x1E, 0) == 0);
    keyboard_reset_state();
    expect_key(0x1E, 'a');
    puts("keyboard: tests passed");
    return 0;
}

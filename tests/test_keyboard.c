#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "keyboard.h"

static char decode(uint8_t scancode, int expected_event) {
    char key = 0;
    uint8_t produced = keyboard_decode(scancode, &key);
    assert(produced == (uint8_t)expected_event);
    return key;
}

static void expect_key(uint8_t scancode, char expected) {
    assert(decode(scancode, 1) == expected);
}

static void expect_ignored(uint8_t scancode) {
    (void)decode(scancode, 0);
}

int main(void) {
    keyboard_reset();

    expect_key(0x1E, 'a');
    expect_ignored(0x9E); /* Key release is never text. */
    expect_key(0x30, 'b');

    expect_ignored(0x2A); /* Left Shift down. */
    expect_key(0x1E, 'A');
    expect_key(0x02, '!');
    expect_key(0x1A, '{');
    expect_key(0x28, '"');
    expect_ignored(0xAA); /* Left Shift up. */
    expect_key(0x1E, 'a');
    expect_key(0x02, '1');

    expect_ignored(0x36); /* Right Shift is independent. */
    expect_key(0x33, '<');
    expect_ignored(0xB6);
    expect_key(0x33, ',');

    expect_ignored(0x3A); /* Caps Lock toggles only on the down edge. */
    expect_ignored(0x3A); /* A repeated make must not toggle it twice. */
    expect_key(0x1E, 'A');
    expect_ignored(0xBA);
    expect_ignored(0x2A);
    expect_key(0x1E, 'a'); /* Shift XOR Caps Lock. */
    expect_ignored(0xAA);
    expect_ignored(0x3A);
    expect_ignored(0xBA);
    expect_key(0x1E, 'a');

    expect_key(0x01, 27);
    expect_key(0x0F, '\t');
    expect_key(0x1C, '\r');
    expect_key(0x0E, '\b');
    expect_key(0x39, ' ');

    expect_ignored(0xE0);
    expect_key(0x48, (char)0x80); /* Up arrow. */
    expect_ignored(0xE0);
    expect_ignored(0xC8); /* Extended key release. */
    expect_ignored(0xE0);
    expect_key(0x50, (char)0x81); /* Down arrow. */
    expect_ignored(0xE0);
    expect_ignored(0x4B); /* Left arrow is not yet part of the GUI ABI. */
    expect_key(0x1E, 'a'); /* An ignored extended key cannot poison next input. */

    keyboard_reset();
    expect_key(0x1E, 'a');
    puts("keyboard: tests passed");
    return 0;
}

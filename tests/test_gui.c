#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"
#include "keyboard.h"
#include "fs.h"

static void row_text(uint8_t row, char output[81]) {
    uint8_t column;
    for (column = 0; column < 80u; column++)
        output[column] = (char)(asteros_gui_test_vga[(uint16_t)row * 80u +
                                                       column] & 0xFFu);
    output[80] = '\0';
}

static uint8_t row_contains(uint8_t row, const char *needle) {
    char text[81];
    row_text(row, text);
    return (uint8_t)(strstr(text, needle) != 0);
}

static void type_text(const char *text) {
    while (*text) gui_handle_key(*text++);
}

static void erase_filename(void) {
    uint8_t count;
    for (count = 0; count < 8u; count++) gui_handle_key('\b');
}

static void test_f5_diagnostic_and_cursor_navigation(void) {
    uint16_t cursor_cell;

    gui_handle_key('i');
    assert(row_contains(5, "TinyLang source editor"));
    gui_handle_key((char)ASTEROS_KEY_CHECK);
    assert(row_contains(18, "Syntax/labels OK"));

    /* Prefixing the comment with an instruction creates a deterministic
       unknown-statement diagnostic on source line 1. */
    gui_handle_key('b');
    gui_handle_key('a');
    gui_handle_key('d');
    gui_handle_key((char)ASTEROS_KEY_CHECK);
    assert(row_contains(18, "Line 1: Unknown or malformed statement"));
    cursor_cell = asteros_gui_test_vga[8u * 80u + 27u];
    assert((cursor_cell & 0xFFu) == 'b');
    assert((cursor_cell >> 8) == 0x71u); /* Diagnostic line cursor highlight. */
}

static void test_save_dirty_guard_and_open_round_trip(void) {
    uint8_t contents[ASTEROS_FS_FILE_MAX];
    uint16_t length = 0;

    /* Save As replaces the initial filename with TEST.TL. */
    gui_handle_key((char)ASTEROS_KEY_SAVE_AS);
    erase_filename();
    type_text("TEST.TL");
    gui_handle_key('\r');
    assert(row_contains(18, "Saved in RAM"));
    assert(asteros_fs_read("TEST.TL", contents, sizeof contents, &length) ==
           ASTEROS_FS_OK);
    assert(length >= 3u && contents[0] == 'b' &&
           contents[1] == 'a' && contents[2] == 'd');

    /* A dirty editor must not enter the open prompt or replace its buffer. */
    gui_handle_key('x');
    gui_handle_key((char)ASTEROS_KEY_OPEN);
    assert(row_contains(18, "Unsaved edits"));
    assert(row_contains(19, "F5 Check syntax"));

    /* Save the edit, then open the original seeded HELLO.TL. */
    gui_handle_key((char)ASTEROS_KEY_SAVE);
    assert(!row_contains(18, "Unsaved edits"));
    gui_handle_key((char)ASTEROS_KEY_OPEN);
    erase_filename();
    type_text("HELLO.TL");
    gui_handle_key('\r');
    assert(row_contains(18, "Loaded from volatile RAM"));
    assert((asteros_gui_test_vga[8u * 80u + 27u] & 0xFFu) == '#');
}

int main(void) {
    asteros_fs_init();
    gui_init();
    test_f5_diagnostic_and_cursor_navigation();
    test_save_dirty_guard_and_open_round_trip();
    puts("gui: IDE validation and file workflow tests passed");
    return 0;
}

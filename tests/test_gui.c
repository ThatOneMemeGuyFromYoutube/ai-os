#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "gui.h"
#include "keyboard.h"
#include "desktop_input.h"
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

static void test_volume_presence_is_reported_without_claiming_persistence(void) {
    gui_init();
    gui_set_ata_disk_present(1);
    gui_set_asteros_volume_present(1);
    gui_handle_key('f');
    assert(row_contains(6, "AsterOS volume found"));
    assert(row_contains(6, "read-only"));
    assert(row_contains(6, "volatile"));

    gui_init();
    gui_set_ata_disk_present(1);
    gui_set_asteros_volume_present(0);
    gui_handle_key('f');
    assert(row_contains(6, "no AsterOS volume"));
    assert(row_contains(6, "volatile"));

    gui_init();
    gui_set_ata_disk_present(0);
    gui_set_asteros_volume_present(0);
    gui_handle_key('f');
    assert(row_contains(6, "no ATA disk detected"));
    assert(row_contains(6, "volatile"));
}

static void test_start_menu_keyboard_and_run_dialog(void) {
    uint8_t index;

    gui_init();
    gui_handle_key((char)ASTEROS_KEY_START_MENU);
    assert(row_contains(14, "AsterOS Start"));
    assert(row_contains(15, "Terminal"));
    assert(row_contains(21, "Run..."));

    /* Up wraps from the first app to Run; Escape cancels without launching. */
    gui_handle_key((char)ASTEROS_KEY_UP);
    gui_handle_key('\r');
    assert(row_contains(9, "Run AsterOS application"));
    type_text("not-an-app");
    gui_handle_key('\r');
    assert(row_contains(14, "Unknown app"));
    gui_handle_key(27);
    assert(row_contains(5, "Select an application"));

    /* Run accepts only a known built-in app and switches to its real window. */
    gui_handle_key((char)ASTEROS_KEY_START_MENU);
    for(index=0;index<ASTEROS_START_MENU_ITEM_RUN;index++)
        gui_handle_key((char)ASTEROS_KEY_DOWN);
    gui_handle_key('\r');
    type_text("  FILES  ");
    gui_handle_key('\r');
    assert(row_contains(4, "Files"));

    /* Cancelling Run preserves the current foreground application. */
    gui_handle_key((char)ASTEROS_KEY_START_MENU);
    for(index=0;index<ASTEROS_START_MENU_ITEM_RUN;index++)
        gui_handle_key((char)ASTEROS_KEY_DOWN);
    gui_handle_key('\r');
    gui_handle_key(27);
    assert(row_contains(4, "Files"));
}

static void test_start_button_mouse_and_menu_item(void) {
    gui_init();
    /* Initial pointer is at cell (40,12); move to the Start taskbar button. */
    gui_handle_mouse(-128,-42,0);
    gui_handle_mouse(-28,0,0);
    gui_handle_mouse(0,0,0);
    gui_handle_mouse(0,0,1);
    assert(row_contains(14, "AsterOS Start"));

    /* Select the IDE row with the mouse and launch it. */
    gui_handle_mouse(0,0,0);
    gui_handle_mouse(28,20,0);
    gui_handle_mouse(0,0,1);
    assert(row_contains(4, "IDE"));
}

static void test_application_help_and_shared_program_catalog(void) {
    uint8_t index;
    const char *names[] = {"Terminal","Files","Programs","About","Settings","IDE"};

    gui_init();
    gui_handle_key('t');
    assert(row_contains(5,"AsterOS terminal"));
    type_text("help files");
    gui_handle_key('\\r');
    assert(row_contains(9,"Files"));
    assert(row_contains(10,"Browse and open files"));
    assert(row_contains(13,"no disk mount"));

    type_text("help ide");
    gui_handle_key('\\r');
    assert(row_contains(9,"IDE"));
    assert(row_contains(10,"Edit TinyLang source"));
    assert(row_contains(13,"F5 does not compile"));

    type_text("help control panel");
    gui_handle_key('\\r');
    assert(row_contains(9,"Settings"));
    assert(row_contains(11,"V visibility"));

    type_text("help unknown");
    gui_handle_key('\\r');
    assert(row_contains(9,"Unknown application topic"));

    type_text("apps");
    gui_handle_key('\\r');
    for(index=0;index<ASTEROS_APP_COUNT;index++)
        assert(row_contains((uint8_t)(9u+index),names[index]));
    assert(row_contains(17,"help <app>"));

    type_text("menu");
    gui_handle_key('\\r');
    gui_handle_key('p');
    assert(row_contains(7,"BUILT-IN APPLICATIONS"));
    for(index=0;index<ASTEROS_APP_COUNT;index++)
        assert(row_contains((uint8_t)(8u+index),names[index]));
    assert(row_contains(16,"help <app>"));
}
int main(void) {
    asteros_fs_init();
    gui_init();
    test_f5_diagnostic_and_cursor_navigation();
    test_save_dirty_guard_and_open_round_trip();
    test_volume_presence_is_reported_without_claiming_persistence();
    test_start_menu_keyboard_and_run_dialog();
    test_start_button_mouse_and_menu_item();
    test_application_help_and_shared_program_catalog();
    puts("gui: IDE, application help, and file workflow tests passed");
    return 0;
}

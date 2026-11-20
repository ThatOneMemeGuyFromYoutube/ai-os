#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "editor.h"

static void test_initial_text_and_lines(void) {
    asteros_editor_t editor;

    asteros_editor_init(&editor, "mov eax, 1\nadd eax, 2\nexit");
    assert(strcmp(asteros_editor_text(&editor),
                  "mov eax, 1\nadd eax, 2\nexit") == 0);
    assert(asteros_editor_line_count(&editor) == 3u);
    assert(asteros_editor_line_start(&editor, 0) == 0u);
    assert(asteros_editor_line_length(&editor, 0) == 10u);
    assert(asteros_editor_line_start(&editor, 1) == 11u);
    assert(asteros_editor_line_length(&editor, 1) == 10u);
    assert(asteros_editor_line_start(&editor, 3) == editor.length);
    assert(!asteros_editor_is_dirty(&editor));

    asteros_editor_init(&editor, "last line\n");
    assert(asteros_editor_line_count(&editor) == 2u);
    assert(asteros_editor_line_length(&editor, 1) == 0u);
}

static void test_insert_delete_and_backspace(void) {
    asteros_editor_t editor;

    asteros_editor_init(&editor, "ac");
    asteros_editor_move_right(&editor);
    assert(asteros_editor_insert(&editor, 'b'));
    assert(strcmp(asteros_editor_text(&editor), "abc") == 0);
    assert(asteros_editor_is_dirty(&editor));

    assert(asteros_editor_delete(&editor));
    assert(strcmp(asteros_editor_text(&editor), "ab") == 0);
    assert(asteros_editor_backspace(&editor));
    assert(strcmp(asteros_editor_text(&editor), "a") == 0);
    assert(asteros_editor_backspace(&editor));
    assert(strcmp(asteros_editor_text(&editor), "") == 0);
    assert(!asteros_editor_backspace(&editor));

    asteros_editor_move_end(&editor);
    assert(!asteros_editor_delete(&editor));
    assert(asteros_editor_backspace(&editor));
    assert(strcmp(asteros_editor_text(&editor), "") == 0);
}

static void test_cursor_movement_and_preferred_column(void) {
    asteros_editor_t editor;

    asteros_editor_init(&editor, "abcd\nxy\n12345");
    asteros_editor_move_end(&editor);
    assert(asteros_editor_cursor_line(&editor) == 0u);
    assert(asteros_editor_cursor_column(&editor) == 4u);

    asteros_editor_move_down(&editor);
    assert(asteros_editor_cursor_line(&editor) == 1u);
    assert(asteros_editor_cursor_column(&editor) == 2u);
    asteros_editor_move_down(&editor);
    assert(asteros_editor_cursor_line(&editor) == 2u);
    assert(asteros_editor_cursor_column(&editor) == 4u);
    asteros_editor_move_up(&editor);
    assert(asteros_editor_cursor_line(&editor) == 1u);
    assert(asteros_editor_cursor_column(&editor) == 2u);
    asteros_editor_move_up(&editor);
    assert(asteros_editor_cursor_line(&editor) == 0u);
    assert(asteros_editor_cursor_column(&editor) == 4u);

    asteros_editor_move_home(&editor);
    assert(asteros_editor_cursor_column(&editor) == 0u);
    asteros_editor_move_left(&editor);
    assert(asteros_editor_cursor_column(&editor) == 0u);
    asteros_editor_move_right(&editor);
    assert(asteros_editor_cursor_column(&editor) == 1u);
}

static void test_newline_and_bounds(void) {
    asteros_editor_t editor;
    char full[ASTEROS_EDITOR_CAPACITY];

    asteros_editor_init(&editor, "");
    assert(asteros_editor_insert(&editor, 'a'));
    assert(asteros_editor_insert(&editor, '\n'));
    assert(asteros_editor_insert(&editor, 'b'));
    assert(strcmp(asteros_editor_text(&editor), "a\nb") == 0);
    assert(asteros_editor_line_count(&editor) == 2u);
    assert(!asteros_editor_insert(&editor, '\t'));
    assert(!asteros_editor_insert(&editor, 27));

    memset(full, 'x', sizeof full - 1u);
    full[sizeof full - 1u] = '\0';
    asteros_editor_init(&editor, full);
    assert(editor.length == ASTEROS_EDITOR_CAPACITY - 1u);
    assert(!asteros_editor_insert(&editor, 'y'));
    assert(editor.length == ASTEROS_EDITOR_CAPACITY - 1u);
    asteros_editor_move_home(&editor);
    assert(asteros_editor_delete(&editor));
    assert(asteros_editor_insert(&editor, 'y'));
    assert(editor.length == ASTEROS_EDITOR_CAPACITY - 1u);
    assert(editor.text[editor.length] == '\0');
}

static void test_noop_operations_preserve_dirty_state(void) {
    asteros_editor_t editor;

    asteros_editor_init(&editor, "x");
    asteros_editor_move_home(&editor);
    assert(!asteros_editor_backspace(&editor));
    asteros_editor_move_end(&editor);
    assert(!asteros_editor_delete(&editor));
    assert(!asteros_editor_is_dirty(&editor));
    assert(!asteros_editor_insert(&editor, '\t'));
    assert(!asteros_editor_is_dirty(&editor));
}

int main(void) {
    test_initial_text_and_lines();
    test_insert_delete_and_backspace();
    test_cursor_movement_and_preferred_column();
    test_newline_and_bounds();
    test_noop_operations_preserve_dirty_state();
    assert(asteros_editor_text(0)[0] == '\0');
    puts("editor: tests passed");
    return 0;
}

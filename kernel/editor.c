#include "editor.h"

static uint16_t line_start_for(const asteros_editor_t *editor,
                               uint16_t requested_line) {
    uint16_t index;
    uint16_t line = 0;
    uint16_t start = 0;

    if (!editor) return 0;
    for (index = 0; index < editor->length; index++) {
        if (line == requested_line) return start;
        if (editor->text[index] == '\n') {
            line++;
            start = (uint16_t)(index + 1u);
        }
    }
    return line == requested_line ? start : editor->length;
}

static uint16_t line_length_for(const asteros_editor_t *editor,
                                uint16_t requested_line) {
    uint16_t start;
    uint16_t length = 0;

    if (!editor || requested_line >= asteros_editor_line_count(editor)) return 0;
    start = line_start_for(editor, requested_line);
    while ((uint16_t)(start + length) < editor->length &&
           editor->text[start + length] != '\n')
        length++;
    return length;
}

void asteros_editor_init(asteros_editor_t *editor, const char *initial_text) {
    uint16_t length = 0;

    if (!editor) return;
    if (initial_text) {
        while (initial_text[length] &&
               length < (uint16_t)(ASTEROS_EDITOR_CAPACITY - 1u)) {
            editor->text[length] = initial_text[length];
            length++;
        }
    }
    editor->text[length] = '\0';
    editor->length = length;
    editor->cursor = 0;
    editor->preferred_column = 0;
    editor->preferred_column_valid = 0;
    editor->dirty = 0;
}

uint8_t asteros_editor_insert(asteros_editor_t *editor, char value) {
    uint16_t index;

    if (!editor || editor->length >= ASTEROS_EDITOR_CAPACITY - 1u)
        return 0;
    if (value != '\n' && (value < 32 || value > 126)) return 0;

    for (index = (uint16_t)(editor->length + 1u);
         index > editor->cursor; index--)
        editor->text[index] = editor->text[index - 1u];
    editor->text[editor->cursor] = value;
    editor->cursor++;
    editor->length++;
    editor->dirty = 1;
    editor->preferred_column_valid = 0;
    return 1;
}

uint8_t asteros_editor_backspace(asteros_editor_t *editor) {
    uint16_t index;

    if (!editor || editor->cursor == 0u) return 0;
    for (index = (uint16_t)(editor->cursor - 1u);
         index < editor->length; index++)
        editor->text[index] = editor->text[index + 1u];
    editor->cursor--;
    editor->length--;
    editor->dirty = 1;
    editor->preferred_column_valid = 0;
    return 1;
}

uint8_t asteros_editor_delete(asteros_editor_t *editor) {
    uint16_t index;

    if (!editor || editor->cursor >= editor->length) return 0;
    for (index = editor->cursor; index < editor->length; index++)
        editor->text[index] = editor->text[index + 1u];
    editor->length--;
    editor->dirty = 1;
    editor->preferred_column_valid = 0;
    return 1;
}

void asteros_editor_move_left(asteros_editor_t *editor) {
    if (!editor) return;
    if (editor->cursor) editor->cursor--;
    editor->preferred_column_valid = 0;
}

void asteros_editor_move_right(asteros_editor_t *editor) {
    if (!editor) return;
    if (editor->cursor < editor->length) editor->cursor++;
    editor->preferred_column_valid = 0;
}

void asteros_editor_move_up(asteros_editor_t *editor) {
    uint16_t line;
    uint16_t column;
    uint16_t start;
    uint16_t length;

    if (!editor) return;
    line = asteros_editor_cursor_line(editor);
    if (!editor->preferred_column_valid) {
        editor->preferred_column = asteros_editor_cursor_column(editor);
        editor->preferred_column_valid = 1;
    }
    if (line == 0u) return;

    start = line_start_for(editor, (uint16_t)(line - 1u));
    length = line_length_for(editor, (uint16_t)(line - 1u));
    column = editor->preferred_column < length
        ? editor->preferred_column : length;
    editor->cursor = (uint16_t)(start + column);
}

void asteros_editor_move_down(asteros_editor_t *editor) {
    uint16_t line;
    uint16_t column;
    uint16_t start;
    uint16_t length;

    if (!editor) return;
    line = asteros_editor_cursor_line(editor);
    if (!editor->preferred_column_valid) {
        editor->preferred_column = asteros_editor_cursor_column(editor);
        editor->preferred_column_valid = 1;
    }
    if ((uint16_t)(line + 1u) >= asteros_editor_line_count(editor)) return;

    start = line_start_for(editor, (uint16_t)(line + 1u));
    length = line_length_for(editor, (uint16_t)(line + 1u));
    column = editor->preferred_column < length
        ? editor->preferred_column : length;
    editor->cursor = (uint16_t)(start + column);
}

void asteros_editor_move_home(asteros_editor_t *editor) {
    if (!editor) return;
    editor->cursor = line_start_for(editor, asteros_editor_cursor_line(editor));
    editor->preferred_column_valid = 0;
}

void asteros_editor_move_end(asteros_editor_t *editor) {
    uint16_t line;

    if (!editor) return;
    line = asteros_editor_cursor_line(editor);
    editor->cursor = (uint16_t)(line_start_for(editor, line) +
                                line_length_for(editor, line));
    editor->preferred_column_valid = 0;
}

uint16_t asteros_editor_line_count(const asteros_editor_t *editor) {
    uint16_t index;
    uint16_t count = 1u;

    if (!editor) return 0;
    for (index = 0; index < editor->length; index++) {
        if (editor->text[index] == '\n') count++;
    }
    return count;
}

uint16_t asteros_editor_cursor_line(const asteros_editor_t *editor) {
    uint16_t index;
    uint16_t line = 0;

    if (!editor) return 0;
    for (index = 0; index < editor->cursor && index < editor->length; index++) {
        if (editor->text[index] == '\n') line++;
    }
    return line;
}

uint16_t asteros_editor_cursor_column(const asteros_editor_t *editor) {
    uint16_t index;
    uint16_t column = 0;

    if (!editor) return 0;
    index = editor->cursor;
    if (index > editor->length) index = editor->length;
    while (index && editor->text[index - 1u] != '\n') {
        index--;
        column++;
    }
    return column;
}

uint16_t asteros_editor_line_start(const asteros_editor_t *editor,
                                   uint16_t line) {
    if (!editor || line >= asteros_editor_line_count(editor))
        return editor ? editor->length : 0;
    return line_start_for(editor, line);
}

uint16_t asteros_editor_line_length(const asteros_editor_t *editor,
                                    uint16_t line) {
    return line_length_for(editor, line);
}

const char *asteros_editor_text(const asteros_editor_t *editor) {
    static const char empty[] = "";
    return editor ? editor->text : empty;
}

uint8_t asteros_editor_is_dirty(const asteros_editor_t *editor) {
    return (uint8_t)(editor && editor->dirty);
}

uint8_t asteros_editor_load(asteros_editor_t *editor, const char *source,
                            uint16_t length) {
    uint16_t index;

    if (!editor || (length && !source) ||
        length >= (uint16_t)ASTEROS_EDITOR_CAPACITY)
        return 0;
    for (index = 0; index < length; index++) {
        char value = source[index];
        if (value != '\n' && (value < 32 || value > 126)) return 0;
    }
    for (index = 0; index < length; index++)
        editor->text[index] = source[index];
    editor->text[length] = '\0';
    editor->length = length;
    editor->cursor = 0;
    editor->preferred_column = 0;
    editor->preferred_column_valid = 0;
    editor->dirty = 0;
    return 1;
}

void asteros_editor_mark_clean(asteros_editor_t *editor) {
    if (editor) editor->dirty = 0;
}

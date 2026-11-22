#ifndef ASTEROS_EDITOR_H
#define ASTEROS_EDITOR_H

#include <stdint.h>

#define ASTEROS_EDITOR_CAPACITY 1024u

typedef struct {
    char text[ASTEROS_EDITOR_CAPACITY];
    uint16_t length;
    uint16_t cursor;
    uint16_t preferred_column;
    uint8_t preferred_column_valid;
    uint8_t dirty;
} asteros_editor_t;

/* A fixed-capacity, allocation-free text buffer for the VGA IDE application.
   The buffer is volatile: this module does not claim to provide file I/O. */
void asteros_editor_init(asteros_editor_t *editor, const char *initial_text);
uint8_t asteros_editor_insert(asteros_editor_t *editor, char value);
uint8_t asteros_editor_backspace(asteros_editor_t *editor);
uint8_t asteros_editor_delete(asteros_editor_t *editor);
void asteros_editor_move_left(asteros_editor_t *editor);
void asteros_editor_move_right(asteros_editor_t *editor);
void asteros_editor_move_up(asteros_editor_t *editor);
void asteros_editor_move_down(asteros_editor_t *editor);
void asteros_editor_move_home(asteros_editor_t *editor);
void asteros_editor_move_end(asteros_editor_t *editor);
/* Move to the start of a zero-based line, clamping to the last line. */
void asteros_editor_move_to_line(asteros_editor_t *editor, uint16_t line);

uint16_t asteros_editor_line_count(const asteros_editor_t *editor);
uint16_t asteros_editor_cursor_line(const asteros_editor_t *editor);
uint16_t asteros_editor_cursor_column(const asteros_editor_t *editor);
uint16_t asteros_editor_line_start(const asteros_editor_t *editor,
                                   uint16_t line);
uint16_t asteros_editor_line_length(const asteros_editor_t *editor,
                                    uint16_t line);
const char *asteros_editor_text(const asteros_editor_t *editor);
uint8_t asteros_editor_is_dirty(const asteros_editor_t *editor);
/* Replace the buffer only when the complete source is valid and fits. */
uint8_t asteros_editor_load(asteros_editor_t *editor, const char *source,
                            uint16_t length);
void asteros_editor_mark_clean(asteros_editor_t *editor);

#endif

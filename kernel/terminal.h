#ifndef ASTEROS_TERMINAL_H
#define ASTEROS_TERMINAL_H

#include <stddef.h>

#define ASTEROS_TERMINAL_LINE_MAX 46u

typedef enum {
    ASTEROS_COMMAND_EMPTY = 0,
    ASTEROS_COMMAND_HELP,
    ASTEROS_COMMAND_CLEAR,
    ASTEROS_COMMAND_APPS,
    ASTEROS_COMMAND_INFO,
    ASTEROS_COMMAND_VERSION,
    ASTEROS_COMMAND_UNAME,
    ASTEROS_COMMAND_PWD,
    ASTEROS_COMMAND_HISTORY,
    ASTEROS_COMMAND_HISTORY_CLEAR,
    ASTEROS_COMMAND_ECHO,
    ASTEROS_COMMAND_MENU,
    ASTEROS_COMMAND_REPEAT,
    ASTEROS_COMMAND_UNKNOWN
} asteros_command_t;

typedef struct {
    char last_command[ASTEROS_TERMINAL_LINE_MAX + 1u];
} asteros_terminal_history_t;

/* Parse one command line without modifying it. For echo, arguments points
   into line after the command and leading argument whitespace; otherwise it
   points to an empty string. Command names are ASCII case-insensitive. */
asteros_command_t terminal_parse_command(const char *line, const char **arguments);

/* The GUI shell keeps only one previous command. Failed repeats leave both
   the input buffer and history unchanged; record truncates to the UI's limit. */
void terminal_history_clear(asteros_terminal_history_t *history);
const char *terminal_history_last(const asteros_terminal_history_t *history);
void terminal_history_record(asteros_terminal_history_t *history, const char *line);
int terminal_history_repeat(const asteros_terminal_history_t *history,
                            char *line, size_t capacity);

#endif

#include "terminal.h"
#include <stddef.h>

static int is_space(char value) {
    return value == ' ' || value == '\t';
}

static char ascii_lower(char value) {
    if (value >= 'A' && value <= 'Z') return (char)(value + ('a' - 'A'));
    return value;
}

static const char *skip_spaces(const char *value) {
    while (is_space(*value)) value++;
    return value;
}

static int token_equals(const char *token, size_t length, const char *expected) {
    size_t index = 0;
    while (expected[index]) {
        if (index >= length || ascii_lower(token[index]) != expected[index]) return 0;
        index++;
    }
    return index == length;
}

asteros_command_t terminal_parse_command(const char *line, const char **arguments) {
    static const char empty[] = "";
    const char *start;
    const char *end;
    const char *tail;
    size_t length;

    if (arguments) *arguments = empty;
    if (!line) return ASTEROS_COMMAND_EMPTY;

    start = skip_spaces(line);
    if (!*start) return ASTEROS_COMMAND_EMPTY;

    end = start;
    while (*end && !is_space(*end)) end++;
    length = (size_t)(end - start);
    tail = skip_spaces(end);

    if (token_equals(start, length, "!!")) {
        return *tail ? ASTEROS_COMMAND_UNKNOWN : ASTEROS_COMMAND_REPEAT;
    }

    if (token_equals(start, length, "echo")) {
        if (arguments) *arguments = tail;
        return ASTEROS_COMMAND_ECHO;
    }

    if (token_equals(start, length, "history")) {
        if (!*tail) return ASTEROS_COMMAND_HISTORY;

        start = tail;
        end = start;
        while (*end && !is_space(*end)) end++;
        if (token_equals(start, (size_t)(end - start), "clear") &&
            !*skip_spaces(end)) {
            return ASTEROS_COMMAND_HISTORY_CLEAR;
        }
        return ASTEROS_COMMAND_UNKNOWN;
    }

    /* Every command other than echo and history takes no arguments. */
    if (*tail) return ASTEROS_COMMAND_UNKNOWN;

    if (token_equals(start, length, "help")) return ASTEROS_COMMAND_HELP;
    if (token_equals(start, length, "clear") || token_equals(start, length, "cls"))
        return ASTEROS_COMMAND_CLEAR;
    if (token_equals(start, length, "apps") || token_equals(start, length, "ls"))
        return ASTEROS_COMMAND_APPS;
    if (token_equals(start, length, "info") || token_equals(start, length, "about"))
        return ASTEROS_COMMAND_INFO;
    if (token_equals(start, length, "ver")) return ASTEROS_COMMAND_VERSION;
    if (token_equals(start, length, "uname")) return ASTEROS_COMMAND_UNAME;
    if (token_equals(start, length, "pwd")) return ASTEROS_COMMAND_PWD;
    if (token_equals(start, length, "menu") || token_equals(start, length, "exit"))
        return ASTEROS_COMMAND_MENU;

    return ASTEROS_COMMAND_UNKNOWN;
}
/* The GUI currently retains one command. Keep this state logic independent
   from VGA rendering so it can be tested on the host. */
void terminal_history_clear(asteros_terminal_history_t *history) {
    if (!history) return;
    history->last_command[0] = '\0';
}

const char *terminal_history_last(const asteros_terminal_history_t *history) {
    static const char empty[] = "";
    return history ? history->last_command : empty;
}

void terminal_history_record(asteros_terminal_history_t *history, const char *line) {
    size_t length = 0;

    if (!history || !line || !*line) return;
    while (length < ASTEROS_TERMINAL_LINE_MAX && line[length]) {
        history->last_command[length] = line[length];
        length++;
    }
    history->last_command[length] = '\0';
}

int terminal_history_repeat(const asteros_terminal_history_t *history,
                            char *line, size_t capacity) {
    const char *last = terminal_history_last(history);
    size_t length = 0;
    size_t index;

    if (!line || capacity == 0 || !*last) return 0;

    /* The backing field is bounded; reject malformed state rather than
       reading beyond it or partially overwriting the caller's input. */
    while (length <= ASTEROS_TERMINAL_LINE_MAX && last[length]) length++;
    if (length == 0 || length > ASTEROS_TERMINAL_LINE_MAX || length >= capacity)
        return 0;

    for (index = 0; index < length; index++) line[index] = last[index];
    line[length] = '\0';
    return 1;
}

#ifndef ASTEROS_TERMINAL_H
#define ASTEROS_TERMINAL_H

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

/* Parse one command line without modifying it. For echo, arguments points
   into line after the command and leading argument whitespace; otherwise it
   points to an empty string. Command names are ASCII case-insensitive. */
asteros_command_t terminal_parse_command(const char *line, const char **arguments);

#endif

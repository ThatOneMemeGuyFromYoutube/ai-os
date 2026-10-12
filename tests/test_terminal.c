#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "terminal.h"

static void expect_command(const char *line, asteros_command_t expected,
                           const char *expected_arguments) {
    const char *arguments = 0;
    asteros_command_t actual = terminal_parse_command(line, &arguments);
    assert(actual == expected);
    assert(arguments != 0);
    assert(strcmp(arguments, expected_arguments) == 0);
}

int main(void) {
    expect_command("", ASTEROS_COMMAND_EMPTY, "");
    expect_command(" \t  ", ASTEROS_COMMAND_EMPTY, "");
    expect_command("  HeLp   ", ASTEROS_COMMAND_HELP, "");
    expect_command("clear", ASTEROS_COMMAND_CLEAR, "");
    expect_command("CLS ", ASTEROS_COMMAND_CLEAR, "");
    expect_command("apps", ASTEROS_COMMAND_APPS, "");
    expect_command("LS", ASTEROS_COMMAND_APPS, "");
    expect_command("info", ASTEROS_COMMAND_INFO, "");
    expect_command("ABOUT", ASTEROS_COMMAND_INFO, "");
    expect_command("ver", ASTEROS_COMMAND_VERSION, "");
    expect_command("UNAME", ASTEROS_COMMAND_UNAME, "");
    expect_command("pwd   ", ASTEROS_COMMAND_PWD, "");
    expect_command("menu", ASTEROS_COMMAND_MENU, "");
    expect_command("EXIT ", ASTEROS_COMMAND_MENU, "");

    expect_command("echo", ASTEROS_COMMAND_ECHO, "");
    expect_command("  ECHO    Hello  World  ", ASTEROS_COMMAND_ECHO,
                   "Hello  World  ");
    expect_command("echo\tAsterOS", ASTEROS_COMMAND_ECHO, "AsterOS");

    expect_command("history", ASTEROS_COMMAND_HISTORY, "");
    expect_command(" HISTORY   clear  ", ASTEROS_COMMAND_HISTORY_CLEAR, "");
    expect_command("history clearx", ASTEROS_COMMAND_UNKNOWN, "");
    expect_command("history clear extra", ASTEROS_COMMAND_UNKNOWN, "");

    expect_command("!!", ASTEROS_COMMAND_REPEAT, "");
    expect_command("  !!   ", ASTEROS_COMMAND_REPEAT, "");
    expect_command("!! now", ASTEROS_COMMAND_UNKNOWN, "");

    /* Recognized commands must not silently accept unsupported arguments. */
    expect_command("help now", ASTEROS_COMMAND_UNKNOWN, "");
    expect_command("pwd /tmp", ASTEROS_COMMAND_UNKNOWN, "");
    expect_command("echoes are not echo", ASTEROS_COMMAND_UNKNOWN, "");
    expect_command("not-a-command", ASTEROS_COMMAND_UNKNOWN, "");
    assert(terminal_parse_command("echo text", 0) == ASTEROS_COMMAND_ECHO);
    assert(terminal_parse_command(0, 0) == ASTEROS_COMMAND_EMPTY);

    puts("terminal: tests passed");
    return 0;
}

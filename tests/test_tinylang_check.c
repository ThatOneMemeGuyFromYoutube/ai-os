#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "tinylang_check.h"

static asteros_tinylang_status_t check(
    const char *source, asteros_tinylang_diagnostic_t *diagnostic) {
    return asteros_tinylang_check(source, (uint16_t)strlen(source), diagnostic);
}

static void test_valid_program_and_forward_labels(void) {
    static const char source[] =
        "# TinyLang guest checker fixture\n"
        "start: mov eax, 0008 ; decimal with leading zeroes\n"
        "mov ecx, 0xFFFFFFFF\n"
        "add eax, 1\n"
        "sub eax, 0x2\n"
        "int 0x80\n"
        "db \"AsterOS\", 0, \"\\n\", 0x41 # quoted bytes\n"
        "jmp done\n"
        "db \"semicolon; and hash # stay in the string\"\n"
        "done: exit\n";
    asteros_tinylang_diagnostic_t diagnostic;
    assert(check(source, &diagnostic) == ASTEROS_TINYLANG_OK);
    assert(diagnostic.status == ASTEROS_TINYLANG_OK);
    assert(diagnostic.line == 0u);
}

static void test_empty_source_and_comments(void) {
    asteros_tinylang_diagnostic_t diagnostic;
    assert(check("", &diagnostic) == ASTEROS_TINYLANG_OK);
    assert(check("  # comment only\n ; another comment\n", &diagnostic) ==
           ASTEROS_TINYLANG_OK);
}

static void test_source_line_diagnostics(void) {
    asteros_tinylang_diagnostic_t diagnostic;
    assert(check("mov eax, 1\njmp missing\nexit\n", &diagnostic) ==
           ASTEROS_TINYLANG_UNKNOWN_LABEL);
    assert(diagnostic.line == 2u);
    assert(check("first: exit\n\nfirst: exit\n", &diagnostic) ==
           ASTEROS_TINYLANG_DUPLICATE_LABEL);
    assert(diagnostic.line == 3u);
    assert(check("mov eax, 1\nmov nope, 2\n", &diagnostic) ==
           ASTEROS_TINYLANG_UNSUPPORTED_REGISTER);
    assert(diagnostic.line == 2u);
    assert(check("mov eax, 1\nadd ebx, 2\n", &diagnostic) ==
           ASTEROS_TINYLANG_MALFORMED_ADD);
    assert(diagnostic.line == 2u);
}

static void test_numbers_and_instruction_ranges(void) {
    asteros_tinylang_diagnostic_t diagnostic;
    assert(check("mov eax, -1\n", &diagnostic) ==
           ASTEROS_TINYLANG_INVALID_NUMBER);
    assert(diagnostic.line == 1u);
    assert(check("mov eax, 4294967296\n", &diagnostic) ==
           ASTEROS_TINYLANG_NUMBER_OUT_OF_RANGE);
    assert(check("int 256\n", &diagnostic) ==
           ASTEROS_TINYLANG_INTERRUPT_OUT_OF_RANGE);
    assert(check("db 256\n", &diagnostic) ==
           ASTEROS_TINYLANG_BYTE_OUT_OF_RANGE);
    assert(check("db 1,,2\n", &diagnostic) ==
           ASTEROS_TINYLANG_MALFORMED_DB);
}

static void test_labels_and_string_escapes(void) {
    asteros_tinylang_diagnostic_t diagnostic;
    assert(check("bad-label: exit\n", &diagnostic) ==
           ASTEROS_TINYLANG_INVALID_LABEL);
    assert(check("jmp 0x100\n", &diagnostic) ==
           ASTEROS_TINYLANG_MALFORMED_JMP);
    assert(check("db \"unterminated\n", &diagnostic) ==
           ASTEROS_TINYLANG_INVALID_STRING);
    assert(check("db \"bad\\q\"\n", &diagnostic) ==
           ASTEROS_TINYLANG_INVALID_STRING);
    assert(check("db \"bad\\xG0\"\n", &diagnostic) ==
           ASTEROS_TINYLANG_INVALID_STRING);
    assert(check("exit 1\n", &diagnostic) ==
           ASTEROS_TINYLANG_UNKNOWN_STATEMENT);
    assert(check("wat\n", &diagnostic) ==
           ASTEROS_TINYLANG_UNKNOWN_STATEMENT);
}

static void test_capacity_and_status_messages(void) {
    char source[ASTEROS_TINYLANG_CHECK_MAX_SOURCE + 2u];
    asteros_tinylang_diagnostic_t diagnostic;
    memset(source, ' ', sizeof source);
    source[sizeof source - 1u] = '\0';
    assert(asteros_tinylang_check(source,
        ASTEROS_TINYLANG_CHECK_MAX_SOURCE, &diagnostic) == ASTEROS_TINYLANG_OK);
    assert(asteros_tinylang_check(source,
        ASTEROS_TINYLANG_CHECK_MAX_SOURCE + 1u, &diagnostic) ==
        ASTEROS_TINYLANG_SOURCE_TOO_LARGE);
    assert(diagnostic.line == 0u);
    assert(asteros_tinylang_status_message(
        ASTEROS_TINYLANG_UNKNOWN_LABEL)[0] == 'U');
    assert(asteros_tinylang_check(0, 1, &diagnostic) ==
           ASTEROS_TINYLANG_INVALID_ARGUMENT);
}

int main(void) {
    test_valid_program_and_forward_labels();
    test_empty_source_and_comments();
    test_source_line_diagnostics();
    test_numbers_and_instruction_ranges();
    test_labels_and_string_escapes();
    test_capacity_and_status_messages();
    puts("tinylang-check: tests passed");
    return 0;
}

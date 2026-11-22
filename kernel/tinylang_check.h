#ifndef ASTEROS_TINYLANG_CHECK_H
#define ASTEROS_TINYLANG_CHECK_H

#include <stdint.h>

#define ASTEROS_TINYLANG_CHECK_MAX_SOURCE 1023u

typedef enum {
    ASTEROS_TINYLANG_OK = 0,
    ASTEROS_TINYLANG_INVALID_ARGUMENT,
    ASTEROS_TINYLANG_SOURCE_TOO_LARGE,
    ASTEROS_TINYLANG_INVALID_LABEL,
    ASTEROS_TINYLANG_DUPLICATE_LABEL,
    ASTEROS_TINYLANG_TOO_MANY_LABELS,
    ASTEROS_TINYLANG_TOO_MANY_JUMPS,
    ASTEROS_TINYLANG_MALFORMED_MOV,
    ASTEROS_TINYLANG_UNSUPPORTED_REGISTER,
    ASTEROS_TINYLANG_MALFORMED_ADD,
    ASTEROS_TINYLANG_MALFORMED_SUB,
    ASTEROS_TINYLANG_MALFORMED_INT,
    ASTEROS_TINYLANG_INTERRUPT_OUT_OF_RANGE,
    ASTEROS_TINYLANG_MALFORMED_JMP,
    ASTEROS_TINYLANG_UNKNOWN_LABEL,
    ASTEROS_TINYLANG_MALFORMED_DB,
    ASTEROS_TINYLANG_INVALID_NUMBER,
    ASTEROS_TINYLANG_NUMBER_OUT_OF_RANGE,
    ASTEROS_TINYLANG_BYTE_OUT_OF_RANGE,
    ASTEROS_TINYLANG_INVALID_STRING,
    ASTEROS_TINYLANG_UNKNOWN_STATEMENT
} asteros_tinylang_status_t;

typedef struct {
    asteros_tinylang_status_t status;
    uint16_t line; /* 1-based; zero means no source line is available. */
} asteros_tinylang_diagnostic_t;

/* Validate TinyLang syntax and local/forward label references without
   generating machine code. This allocation-free guest-side checker mirrors
   the host compiler's instruction and db-string grammar. */
asteros_tinylang_status_t asteros_tinylang_check(
    const char *source, uint16_t length,
    asteros_tinylang_diagnostic_t *diagnostic);

const char *asteros_tinylang_status_message(
    asteros_tinylang_status_t status);

#endif

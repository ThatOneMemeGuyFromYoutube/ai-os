#include "tinylang_check.h"

typedef struct {
    const char *data;
    uint16_t length;
} source_slice_t;

typedef struct {
    uint16_t offset;
    uint16_t length;
} source_name_t;

typedef struct {
    uint16_t offset;
    uint16_t length;
    uint16_t line;
} source_reference_t;

#define MAX_LABELS 64u
#define MAX_REFERENCES 64u

static uint8_t is_space(char value) {
    return (uint8_t)(value == ' ' || value == '\t' || value == '\r');
}

static uint8_t is_digit(char value) {
    return (uint8_t)(value >= '0' && value <= '9');
}

static uint8_t is_hex_digit(char value) {
    return (uint8_t)(is_digit(value) ||
        (value >= 'a' && value <= 'f') ||
        (value >= 'A' && value <= 'F'));
}

static uint8_t hex_value(char value) {
    if (value >= '0' && value <= '9') return (uint8_t)(value - '0');
    if (value >= 'a' && value <= 'f') return (uint8_t)(value - 'a' + 10);
    return (uint8_t)(value - 'A' + 10);
}

static uint8_t is_alpha(char value) {
    return (uint8_t)((value >= 'a' && value <= 'z') ||
                     (value >= 'A' && value <= 'Z'));
}

static uint8_t is_ident_start(char value) {
    return (uint8_t)(is_alpha(value) || value == '_');
}

static uint8_t is_ident_part(char value) {
    return (uint8_t)(is_ident_start(value) || is_digit(value));
}

static source_slice_t trim(source_slice_t value) {
    while (value.length && is_space(value.data[0])) {
        value.data++;
        value.length--;
    }
    while (value.length && is_space(value.data[value.length - 1u]))
        value.length--;
    return value;
}

static uint8_t equals(source_slice_t value, const char *literal) {
    uint16_t index = 0;
    while (literal[index]) {
        if (index >= value.length || value.data[index] != literal[index])
            return 0;
        index++;
    }
    return (uint8_t)(index == value.length);
}

static uint8_t slices_equal(source_slice_t left, source_slice_t right) {
    uint16_t index;
    if (left.length != right.length) return 0;
    for (index = 0; index < left.length; index++)
        if (left.data[index] != right.data[index]) return 0;
    return 1;
}

/* Return the first unquoted comment marker, or the complete line length. */
static uint16_t comment_start(source_slice_t line) {
    uint16_t index;
    uint8_t in_string = 0;
    uint8_t escaped = 0;
    for (index = 0; index < line.length; index++) {
        char value = line.data[index];
        if (in_string) {
            if (escaped) escaped = 0;
            else if (value == '\\') escaped = 1;
            else if (value == '"') in_string = 0;
        } else if (value == '"') {
            in_string = 1;
        } else if (value == '#' || value == ';') {
            return index;
        }
    }
    return line.length;
}

/* Find a delimiter outside quoted strings. */
static uint16_t find_unquoted(source_slice_t value, char target) {
    uint16_t index;
    uint8_t in_string = 0;
    uint8_t escaped = 0;
    for (index = 0; index < value.length; index++) {
        char current = value.data[index];
        if (in_string) {
            if (escaped) escaped = 0;
            else if (current == '\\') escaped = 1;
            else if (current == '"') in_string = 0;
        } else if (current == '"') {
            in_string = 1;
        } else if (current == target) {
            return index;
        }
    }
    return value.length;
}

static asteros_tinylang_status_t parse_number(
    source_slice_t token, uint32_t *result) {
    uint16_t index = 0;
    uint8_t base = 10;
    uint32_t value = 0;
    token = trim(token);
    if (!token.length || !result) return ASTEROS_TINYLANG_INVALID_NUMBER;
    if (token.length >= 2u && token.data[0] == '0' &&
        (token.data[1] == 'x' || token.data[1] == 'X')) {
        base = 16;
        index = 2;
        if (index == token.length) return ASTEROS_TINYLANG_INVALID_NUMBER;
    }
    for (; index < token.length; index++) {
        uint8_t digit;
        char current = token.data[index];
        if (base == 10) {
            if (!is_digit(current)) return ASTEROS_TINYLANG_INVALID_NUMBER;
            digit = (uint8_t)(current - '0');
        } else {
            if (!is_hex_digit(current)) return ASTEROS_TINYLANG_INVALID_NUMBER;
            digit = hex_value(current);
        }
        if (base == 10u) {
            if (value > 429496729u ||
                (value == 429496729u && digit > 5u))
                return ASTEROS_TINYLANG_NUMBER_OUT_OF_RANGE;
            value = value * 10u + digit;
        } else {
            if (value > 0x0FFFFFFFu)
                return ASTEROS_TINYLANG_NUMBER_OUT_OF_RANGE;
            value = (value << 4) | digit;
        }
    }
    *result = value;
    return ASTEROS_TINYLANG_OK;
}

static uint8_t valid_identifier(source_slice_t name) {
    uint16_t index;
    if (!name.length || !is_ident_start(name.data[0])) return 0;
    for (index = 1; index < name.length; index++)
        if (!is_ident_part(name.data[index])) return 0;
    return 1;
}

static uint8_t supported_register(source_slice_t name) {
    return (uint8_t)(
        equals(name, "eax") || equals(name, "ecx") ||
        equals(name, "edx") || equals(name, "ebx") ||
        equals(name, "esp") || equals(name, "ebp") ||
        equals(name, "esi") || equals(name, "edi"));
}

static asteros_tinylang_status_t parse_db(source_slice_t arguments) {
    uint16_t cursor = 0;
    arguments = trim(arguments);
    if (!arguments.length) return ASTEROS_TINYLANG_MALFORMED_DB;

    while (cursor < arguments.length) {
        uint32_t number;
        if (is_space(arguments.data[cursor])) {
            cursor++;
            continue;
        }
        if (arguments.data[cursor] == '"') {
            uint8_t closed = 0;
            cursor++;
            while (cursor < arguments.length) {
                char value = arguments.data[cursor++];
                if (value == '"') {
                    closed = 1;
                    break;
                }
                if (value == '\\') {
                    char escape;
                    if (cursor >= arguments.length)
                        return ASTEROS_TINYLANG_INVALID_STRING;
                    escape = arguments.data[cursor++];
                    if (escape == 'x') {
                        if ((uint16_t)(cursor + 2u) > arguments.length ||
                            !is_hex_digit(arguments.data[cursor]) ||
                            !is_hex_digit(arguments.data[cursor + 1u]))
                            return ASTEROS_TINYLANG_INVALID_STRING;
                        cursor = (uint16_t)(cursor + 2u);
                    } else if (escape != 'n' && escape != 'r' &&
                               escape != 't' && escape != '0' &&
                               escape != '"' && escape != '\\') {
                        return ASTEROS_TINYLANG_INVALID_STRING;
                    }
                } else if ((uint8_t)value > 0x7Fu) {
                    return ASTEROS_TINYLANG_INVALID_STRING;
                }
            }
            if (!closed) return ASTEROS_TINYLANG_INVALID_STRING;
        } else {
            uint16_t start = cursor;
            source_slice_t token;
            asteros_tinylang_status_t status;
            while (cursor < arguments.length &&
                   arguments.data[cursor] != ',') {
                if (arguments.data[cursor] == '"')
                    return ASTEROS_TINYLANG_MALFORMED_DB;
                cursor++;
            }
            token.data = arguments.data + start;
            token.length = (uint16_t)(cursor - start);
            token = trim(token);
            if (!token.length) return ASTEROS_TINYLANG_MALFORMED_DB;
            status = parse_number(token, &number);
            if (status != ASTEROS_TINYLANG_OK) return status;
            if (number > 0xFFu) return ASTEROS_TINYLANG_BYTE_OUT_OF_RANGE;
        }

        while (cursor < arguments.length && is_space(arguments.data[cursor]))
            cursor++;
        if (cursor == arguments.length) return ASTEROS_TINYLANG_OK;
        if (arguments.data[cursor] != ',')
            return ASTEROS_TINYLANG_MALFORMED_DB;
        cursor++;
        {
            uint16_t lookahead = cursor;
            while (lookahead < arguments.length &&
                   is_space(arguments.data[lookahead])) lookahead++;
            if (lookahead == arguments.length)
                return ASTEROS_TINYLANG_MALFORMED_DB;
        }
    }
    return ASTEROS_TINYLANG_MALFORMED_DB;
}

static asteros_tinylang_status_t parse_statement(
    source_slice_t statement, source_slice_t *jump_target) {
    uint16_t mnemonic_length = 0;
    source_slice_t mnemonic;
    source_slice_t operands;
    uint16_t comma;
    uint32_t number;
    asteros_tinylang_status_t status;

    statement = trim(statement);
    if (!statement.length) return ASTEROS_TINYLANG_OK;
    while (mnemonic_length < statement.length &&
           is_alpha(statement.data[mnemonic_length]))
        mnemonic_length++;
    mnemonic.data = statement.data;
    mnemonic.length = mnemonic_length;
    operands.data = statement.data + mnemonic_length;
    operands.length = (uint16_t)(statement.length - mnemonic_length);
    operands = trim(operands);

    if (equals(mnemonic, "exit"))
        return operands.length ? ASTEROS_TINYLANG_UNKNOWN_STATEMENT
                               : ASTEROS_TINYLANG_OK;

    if (equals(mnemonic, "mov")) {
        source_slice_t reg;
        comma = find_unquoted(operands, ',');
        if (comma == operands.length) return ASTEROS_TINYLANG_MALFORMED_MOV;
        reg.data = operands.data;
        reg.length = comma;
        reg = trim(reg);
        if (!supported_register(reg))
            return ASTEROS_TINYLANG_UNSUPPORTED_REGISTER;
        operands.data += comma + 1u;
        operands.length = (uint16_t)(operands.length - comma - 1u);
        status = parse_number(operands, &number);
        return status == ASTEROS_TINYLANG_OK ? ASTEROS_TINYLANG_OK : status;
    }

    if (equals(mnemonic, "add") || equals(mnemonic, "sub")) {
        source_slice_t reg;
        comma = find_unquoted(operands, ',');
        if (comma == operands.length)
            return equals(mnemonic, "add") ? ASTEROS_TINYLANG_MALFORMED_ADD
                                            : ASTEROS_TINYLANG_MALFORMED_SUB;
        reg.data = operands.data;
        reg.length = comma;
        reg = trim(reg);
        if (!equals(reg, "eax"))
            return equals(mnemonic, "add") ? ASTEROS_TINYLANG_MALFORMED_ADD
                                            : ASTEROS_TINYLANG_MALFORMED_SUB;
        operands.data += comma + 1u;
        operands.length = (uint16_t)(operands.length - comma - 1u);
        status = parse_number(operands, &number);
        return status;
    }

    if (equals(mnemonic, "int")) {
        if (!operands.length) return ASTEROS_TINYLANG_MALFORMED_INT;
        status = parse_number(operands, &number);
        if (status != ASTEROS_TINYLANG_OK) return status;
        return number > 0xFFu ? ASTEROS_TINYLANG_INTERRUPT_OUT_OF_RANGE
                              : ASTEROS_TINYLANG_OK;
    }

    if (equals(mnemonic, "jmp")) {
        if (!valid_identifier(operands)) return ASTEROS_TINYLANG_MALFORMED_JMP;
        *jump_target = operands;
        return ASTEROS_TINYLANG_OK;
    }

    if (equals(mnemonic, "db")) return parse_db(operands);
    return ASTEROS_TINYLANG_UNKNOWN_STATEMENT;
}

asteros_tinylang_status_t asteros_tinylang_check(
    const char *source, uint16_t length,
    asteros_tinylang_diagnostic_t *diagnostic) {
    source_name_t labels[MAX_LABELS];
    source_reference_t references[MAX_REFERENCES];
    uint16_t label_count = 0;
    uint16_t reference_count = 0;
    uint16_t line_start = 0;
    uint16_t line_number = 1;
    asteros_tinylang_status_t result = ASTEROS_TINYLANG_OK;

    if (diagnostic) {
        diagnostic->status = ASTEROS_TINYLANG_OK;
        diagnostic->line = 0;
    }
    if ((!source && length) || length > ASTEROS_TINYLANG_CHECK_MAX_SOURCE) {
        result = length > ASTEROS_TINYLANG_CHECK_MAX_SOURCE
            ? ASTEROS_TINYLANG_SOURCE_TOO_LARGE
            : ASTEROS_TINYLANG_INVALID_ARGUMENT;
        goto done;
    }
    if (!source) source = "";

    while (line_start < length || (line_start == 0 && length == 0)) {
        uint16_t line_end = line_start;
        source_slice_t line;
        source_slice_t statement;
        uint16_t comment;
        uint16_t colon;
        source_slice_t jump_target = {0, 0};

        while (line_end < length && source[line_end] != '\n') line_end++;
        line.data = source + line_start;
        line.length = (uint16_t)(line_end - line_start);
        comment = comment_start(line);
        line.length = comment;
        line = trim(line);
        if (line.length) {
            colon = find_unquoted(line, ':');
            statement = line;
            if (colon < line.length) {
                source_slice_t label;
                uint16_t index;
                label.data = line.data;
                label.length = colon;
                label = trim(label);
                if (!valid_identifier(label)) {
                    result = ASTEROS_TINYLANG_INVALID_LABEL;
                    goto line_error;
                }
                for (index = 0; index < label_count; index++) {
                    source_slice_t previous;
                    previous.data = source + labels[index].offset;
                    previous.length = labels[index].length;
                    if (slices_equal(previous, label)) {
                        result = ASTEROS_TINYLANG_DUPLICATE_LABEL;
                        goto line_error;
                    }
                }
                if (label_count >= MAX_LABELS) {
                    result = ASTEROS_TINYLANG_TOO_MANY_LABELS;
                    goto line_error;
                }
                labels[label_count].offset =
                    (uint16_t)(label.data - source);
                labels[label_count].length = label.length;
                label_count++;
                statement.data = line.data + colon + 1u;
                statement.length = (uint16_t)(line.length - colon - 1u);
            }
            result = parse_statement(statement, &jump_target);
            if (result != ASTEROS_TINYLANG_OK) goto line_error;
            if (jump_target.length) {
                if (reference_count >= MAX_REFERENCES) {
                    result = ASTEROS_TINYLANG_TOO_MANY_JUMPS;
                    goto line_error;
                }
                references[reference_count].offset =
                    (uint16_t)(jump_target.data - source);
                references[reference_count].length = jump_target.length;
                references[reference_count].line = line_number;
                reference_count++;
            }
        }
        if (line_end >= length) break;
        line_start = (uint16_t)(line_end + 1u);
        line_number++;
    }

    {
        uint16_t reference_index;
        for (reference_index = 0; reference_index < reference_count;
             reference_index++) {
            uint16_t label_index;
            uint8_t found = 0;
            source_slice_t reference;
            reference.data = source + references[reference_index].offset;
            reference.length = references[reference_index].length;
            for (label_index = 0; label_index < label_count; label_index++) {
                source_slice_t label;
                label.data = source + labels[label_index].offset;
                label.length = labels[label_index].length;
                if (slices_equal(reference, label)) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                result = ASTEROS_TINYLANG_UNKNOWN_LABEL;
                if (diagnostic) {
                    diagnostic->status = result;
                    diagnostic->line = references[reference_index].line;
                }
                return result;
            }
        }
    }
    return ASTEROS_TINYLANG_OK;

line_error:
    if (diagnostic) {
        diagnostic->status = result;
        diagnostic->line = line_number;
    }
    return result;

done:
    if (diagnostic) diagnostic->status = result;
    return result;
}

const char *asteros_tinylang_status_message(
    asteros_tinylang_status_t status) {
    switch (status) {
        case ASTEROS_TINYLANG_OK: return "Syntax and labels are valid";
        case ASTEROS_TINYLANG_INVALID_ARGUMENT: return "Invalid source buffer";
        case ASTEROS_TINYLANG_SOURCE_TOO_LARGE: return "Source exceeds 1023 bytes";
        case ASTEROS_TINYLANG_INVALID_LABEL: return "Invalid label name";
        case ASTEROS_TINYLANG_DUPLICATE_LABEL: return "Duplicate label";
        case ASTEROS_TINYLANG_TOO_MANY_LABELS: return "Too many labels (64 max)";
        case ASTEROS_TINYLANG_TOO_MANY_JUMPS: return "Too many jump references";
        case ASTEROS_TINYLANG_MALFORMED_MOV: return "Expected mov register, number";
        case ASTEROS_TINYLANG_UNSUPPORTED_REGISTER: return "Unsupported mov register";
        case ASTEROS_TINYLANG_MALFORMED_ADD: return "Expected add eax, number";
        case ASTEROS_TINYLANG_MALFORMED_INT: return "Expected int number";
        case ASTEROS_TINYLANG_MALFORMED_SUB: return "Expected sub eax, number";
        case ASTEROS_TINYLANG_INTERRUPT_OUT_OF_RANGE: return "Interrupt vector must be 0..255";
        case ASTEROS_TINYLANG_MALFORMED_JMP: return "Expected jmp label";
        case ASTEROS_TINYLANG_UNKNOWN_LABEL: return "Unknown jump label";
        case ASTEROS_TINYLANG_MALFORMED_DB: return "Malformed db byte list";
        case ASTEROS_TINYLANG_INVALID_NUMBER: return "Expected decimal or hex number";
        case ASTEROS_TINYLANG_NUMBER_OUT_OF_RANGE: return "Number exceeds 32 bits";
        case ASTEROS_TINYLANG_BYTE_OUT_OF_RANGE: return "db value must fit in one byte";
        case ASTEROS_TINYLANG_INVALID_STRING: return "Invalid db string or escape";
        case ASTEROS_TINYLANG_UNKNOWN_STATEMENT: return "Unknown or malformed statement";
        default: return "Unknown TinyLang diagnostic";
    }
}

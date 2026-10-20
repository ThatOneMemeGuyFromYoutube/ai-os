#include "keyboard.h"

static uint8_t extended_prefix;
static uint8_t left_shift;
static uint8_t right_shift;
static uint8_t left_ctrl;
static uint8_t right_ctrl;
static uint8_t caps_lock;
static uint8_t caps_down;

static const char normal_map[128] = {
    [0x01] = 27,
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = '\b', [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\r',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = '`',
    [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm', [0x33] = ',',
    [0x34] = '.', [0x35] = '/', [0x39] = ' '
};

static char shifted_punctuation(uint8_t scancode, char fallback) {
    switch (scancode) {
        case 0x02: return '!'; case 0x03: return '@';
        case 0x04: return '#'; case 0x05: return '$';
        case 0x06: return '%'; case 0x07: return '^';
        case 0x08: return '&'; case 0x09: return '*';
        case 0x0A: return '('; case 0x0B: return ')';
        case 0x0C: return '_'; case 0x0D: return '+';
        case 0x1A: return '{'; case 0x1B: return '}';
        case 0x27: return ':'; case 0x28: return '"';
        case 0x29: return '~'; case 0x2B: return '|';
        case 0x33: return '<'; case 0x34: return '>';
        case 0x35: return '?';
        default: return fallback;
    }
}

void keyboard_reset_state(void) {
    extended_prefix = 0;
    left_shift = 0;
    right_shift = 0;
    left_ctrl = 0;
    right_ctrl = 0;
    caps_lock = 0;
    caps_down = 0;
}

uint8_t keyboard_decode_scancode(uint8_t scancode, char *key) {
    uint8_t code;
    uint8_t shift;
    char value;

    if (scancode == 0xE0) {
        extended_prefix = 1;
        return 0;
    }
    if (scancode == 0xE1) {
        extended_prefix = 0;
        return 0;
    }
    code = (uint8_t)(scancode & 0x7F);
    if (scancode & 0x80) {
        if (extended_prefix) {
            extended_prefix = 0;
            if (code == 0x1D) right_ctrl = 0;
            return 0;
        }
        if (code == 0x2A) left_shift = 0;
        else if (code == 0x36) right_shift = 0;
        else if (code == 0x1D) left_ctrl = 0;
        else if (code == 0x3A) caps_down = 0;
        return 0;
    }
    if (extended_prefix) {
        extended_prefix = 0;
        if (code == 0x1D) { right_ctrl = 1; return 0; }
        if (!key) return 0;
        if (code == 0x48) { *key = (char)ASTEROS_KEY_UP; return 1; }
        if (code == 0x50) { *key = (char)ASTEROS_KEY_DOWN; return 1; }
        return 0;
    }
    if (code == 0x1D) { left_ctrl = 1; return 0; }
    if (code == 0x2A) {
        if (!left_shift && key) *key = (char)((left_ctrl || right_ctrl) ? ASTEROS_KEY_HOVER_RIGHT_CLICK : ASTEROS_KEY_HOVER_LEFT_CLICK);
        else return 0;
        left_shift = 1;
        return 1;
    }
    if (code == 0x36) {
        if (!right_shift && key) *key = (char)((left_ctrl || right_ctrl) ? ASTEROS_KEY_HOVER_RIGHT_CLICK : ASTEROS_KEY_HOVER_LEFT_CLICK);
        else return 0;
        right_shift = 1;
        return 1;
    }
    if (code == 0x3A) {
        if (!caps_down) caps_lock = (uint8_t)!caps_lock;
        caps_down = 1;
        return 0;
    }
    if (!key || code >= sizeof(normal_map)) return 0;
    value = normal_map[code];
    if (!value) return 0;
    shift = (uint8_t)(left_shift || right_shift);
    if (value >= 'a' && value <= 'z') {
        if ((uint8_t)(shift ^ caps_lock)) value = (char)(value - 'a' + 'A');
    } else if (shift) value = shifted_punctuation(code, value);
    *key = value;
    return 1;
}

#include <stdint.h>
#include "gui.h"

/* BDOS-like service numbers will live here as the ABI stabilizes. */
enum { SVC_CONSOLE = 1, SVC_FILES = 2, SVC_PROCESS = 3 };

static uint8_t inb(uint16_t port) {
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static char keyboard_getchar(void) {
    static const char normal[] = "?1234567890-=?qwertyuiop[]?asdfghjkl;'`?zxcvbnm,./";
    uint8_t scancode;
    for (;;) {
        if (!(inb(0x64) & 1)) continue;
        scancode = inb(0x60);
        if (scancode & 0x80) continue;
        if (scancode == 0x1C) return '\r';
        if (scancode == 0x10) return 'q';
        if (scancode == 0x11) return 'w';
        if (scancode == 0x1F) return 's';
        if (scancode == 0x1E) return 'a';
        if (scancode < sizeof(normal) - 1) return normal[scancode];
    }
}

void kmain(void) {
    gui_init();
    for (;;) {
        gui_handle_key(keyboard_getchar());
    }
}

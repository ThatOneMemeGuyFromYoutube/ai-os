#include <stdint.h>

#define VGA ((volatile uint16_t*)0xB8000)
static uint16_t cursor;

static void putc(char c) {
    if (c == '\n') { cursor = (uint16_t)((cursor / 80 + 1) * 80); return; }
    VGA[cursor++] = (uint16_t)(0x07 << 8) | (uint8_t)c;
}

static void puts(const char *s) { while (*s) putc(*s++); }

/* BDOS-like service numbers will live here as the ABI stabilizes. */
enum { SVC_CONSOLE = 1, SVC_FILES = 2, SVC_PROCESS = 3 };

static int streq(const char *a, const char *b) {
    while (*a && *b && *a == *b) { ++a; ++b; }
    return *a == *b;
}

static void ccp(void) {
    puts("AsterOS 0.1\n");
    puts("CCP/M-style command processor online.\n");
    puts("Type HELP for the planned command set.\n\n");
    puts("A:> ");
    /* Keyboard, filesystem, and transient-program loading are next milestones. */
    (void)streq;
}

void kmain(void) {
    cursor = 0;
    ccp();
    for (;;) { __asm__ volatile ("hlt"); }
}

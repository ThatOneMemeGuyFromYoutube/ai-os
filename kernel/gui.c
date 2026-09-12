#include "gui.h"

#define VGA ((volatile uint16_t*)0xB8000)
#define WIDTH 80
#define HEIGHT 25

#define ATTR_NORMAL 0x07
#define ATTR_TITLE  0x1F
#define ATTR_PANEL  0x70
#define ATTR_STATUS 0x1E
#define ATTR_SELECT 0x71

static uint8_t selected;
static const char *items[] = { "Terminal", "Files", "Programs", "About" };

static void cell(uint8_t x, uint8_t y, char c, uint8_t attr) {
    if (x < WIDTH && y < HEIGHT) VGA[(uint16_t)y * WIDTH + x] = ((uint16_t)attr << 8) | (uint8_t)c;
}

static void fill(uint8_t x, uint8_t y, uint8_t w, uint8_t h, char c, uint8_t attr) {
    for (uint8_t row = 0; row < h; ++row)
        for (uint8_t col = 0; col < w; ++col)
            cell((uint8_t)(x + col), (uint8_t)(y + row), c, attr);
}

static void text(uint8_t x, uint8_t y, const char *s, uint8_t attr) {
    while (*s && x < WIDTH) cell(x++, y, *s++, attr);
}

static void border(uint8_t x, uint8_t y, uint8_t w, uint8_t h) {
    for (uint8_t i = 0; i < w; ++i) {
        cell((uint8_t)(x + i), y, i == 0 ? '+' : (i == w - 1 ? '+' : '-'), ATTR_PANEL);
        cell((uint8_t)(x + i), (uint8_t)(y + h - 1), i == 0 ? '+' : (i == w - 1 ? '+' : '-'), ATTR_PANEL);
    }
    for (uint8_t i = 1; i + 1 < h; ++i) {
        cell(x, (uint8_t)(y + i), '|', ATTR_PANEL);
        cell((uint8_t)(x + w - 1), (uint8_t)(y + i), '|', ATTR_PANEL);
    }
}

static void draw_terminal(void) {
    text(22, 5, "AsterOS terminal", ATTR_STATUS);
    text(22, 7, "aster> help", ATTR_NORMAL);
    text(22, 8, "  help   show built-in commands", ATTR_NORMAL);
    text(22, 9, "  clear  redraw the GUI shell", ATTR_NORMAL);
    text(22, 10, "  apps   list GUI applications", ATTR_NORMAL);
    text(22, 12, "This view is backed by the keyboard event loop.", ATTR_NORMAL);
}

static void draw_files(void) {
    text(22, 5, "Files", ATTR_STATUS);
    text(22, 7, "NAME                 TYPE", ATTR_NORMAL);
    text(22, 8, "README.MD             text", ATTR_NORMAL);
    text(22, 9, "KERNEL.BIN            system", ATTR_NORMAL);
    text(22, 10, "PROGRAMS/             directory", ATTR_NORMAL);
    text(22, 12, "Filesystem browser placeholder is now an", ATTR_NORMAL);
    text(22, 13, "explicit read-only system view, not a blank panel.", ATTR_NORMAL);
}

static void draw_programs(void) {
    text(22, 5, "Programs", ATTR_STATUS);
    text(22, 7, "BUILT-IN PROGRAMS", ATTR_NORMAL);
    text(22, 8, "- Terminal   keyboard-driven command view", ATTR_NORMAL);
    text(22, 9, "- Files      read-only filesystem view", ATTR_NORMAL);
    text(22, 10, "- Programs   this launcher and inventory", ATTR_NORMAL);
    text(22, 11, "- About      OS/runtime information", ATTR_NORMAL);
}

static void draw_about(void) {
    text(22, 5, "About AsterOS", ATTR_STATUS);
    text(22, 7, "32-bit x86 experimental operating system", ATTR_NORMAL);
    text(22, 8, "GUI shell over the CCP/BDOS direction", ATTR_NORMAL);
    text(22, 10, "Keyboard: W/S or arrow keys, Enter to open", ATTR_NORMAL);
    text(22, 11, "Q returns to the launcher", ATTR_NORMAL);
}

static void draw_selected_app(void) {
    fill(21, 4, 57, 18, ' ', ATTR_NORMAL);
    if (selected == 0) draw_terminal();
    else if (selected == 1) draw_files();
    else if (selected == 2) draw_programs();
    else draw_about();
}

void gui_draw(void) {
    fill(0, 0, WIDTH, HEIGHT, ' ', ATTR_NORMAL);
    fill(0, 0, WIDTH, 1, ' ', ATTR_TITLE);
    text(2, 0, "AsterOS", ATTR_TITLE);
    text(68, 0, "GUI Shell", ATTR_TITLE);

    border(1, 2, 18, 19);
    text(3, 3, "Applications", ATTR_PANEL);
    for (uint8_t i = 0; i < 4; ++i)
        text(3, (uint8_t)(5 + i * 2), items[i], i == selected ? ATTR_SELECT : ATTR_PANEL);

    border(20, 2, 59, 19);
    text(22, 3, "Welcome to AsterOS", ATTR_NORMAL);
    draw_selected_app();

    fill(0, 23, WIDTH, 2, ' ', ATTR_STATUS);
    text(2, 23, "Enter Open   W/S Navigate   Q Launcher", ATTR_STATUS);
}

void gui_init(void) {
    selected = 0;
    gui_draw();
}

void gui_handle_key(char key) {
    if (key == 'w' || key == 'W') {
        if (selected == 0) selected = 3; else --selected;
        gui_draw();
    } else if (key == 's' || key == 'S') {
        selected = (uint8_t)((selected + 1) % 4);
        gui_draw();
    } else if (key == '\r' || key == '\n') {
        draw_selected_app();
    } else if (key == 'q' || key == 'Q') {
        gui_draw();
    }
}

#include "gui.h"

#define VGA ((volatile uint16_t*)0xB8000)
#define WIDTH 80
#define HEIGHT 25

#define ATTR_NORMAL 0x07
#define ATTR_TITLE  0x1F
#define ATTR_PANEL  0x70
#define ATTR_STATUS 0x1E
#define ATTR_SELECT 0x71

static uint16_t cursor;
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
    text(22, 5, "A lightweight GUI shell over the CCP/BDOS core.", ATTR_NORMAL);
    text(22, 7, "Use W/S or arrow keys to select an app.", ATTR_NORMAL);
    text(22, 8, "Press Enter to launch it.", ATTR_NORMAL);
    text(22, 11, "Selected:", ATTR_NORMAL);
    text(32, 11, items[selected], ATTR_STATUS);

    fill(0, 23, WIDTH, 2, ' ', ATTR_STATUS);
    text(2, 23, "F1 Menu   Enter Open   W/S Navigate   Q Shell", ATTR_STATUS);
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
        fill(21, 13, 57, 6, ' ', ATTR_NORMAL);
        text(23, 15, "Application launcher will connect to the", ATTR_NORMAL);
        text(23, 16, "existing transient-program/BDOS services.", ATTR_NORMAL);
    }
}

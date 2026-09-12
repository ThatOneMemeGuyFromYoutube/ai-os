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
static int8_t active_app;
static uint8_t terminal_page;
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
    if (terminal_page == 0) {
        text(22, 7, "aster> help", ATTR_NORMAL);
        text(22, 8, "  h  help      c  clear screen", ATTR_NORMAL);
        text(22, 9, "  a  list apps q  launcher", ATTR_NORMAL);
        text(22, 11, "Press a command key to execute it.", ATTR_NORMAL);
    } else if (terminal_page == 1) {
        text(22, 7, "AsterOS built-in commands", ATTR_NORMAL);
        text(22, 8, "h - show this help", ATTR_NORMAL);
        text(22, 9, "c - clear terminal output", ATTR_NORMAL);
        text(22, 10, "a - list GUI applications", ATTR_NORMAL);
        text(22, 11, "q - return to launcher", ATTR_NORMAL);
    } else {
        text(22, 7, "Applications: Terminal, Files, Programs, About", ATTR_NORMAL);
        text(22, 9, "The GUI shell is interactive; Enter opens an app.", ATTR_NORMAL);
    }
}

static void draw_files(void) {
    text(22, 5, "Files", ATTR_STATUS);
    text(22, 7, "NAME                 TYPE", ATTR_NORMAL);
    text(22, 8, "README.MD             text", ATTR_NORMAL);
    text(22, 9, "KERNEL.BIN            system", ATTR_NORMAL);
    text(22, 10, "PROGRAMS/             directory", ATTR_NORMAL);
    text(22, 12, "Read-only filesystem view", ATTR_NORMAL);
    text(22, 13, "Press Q to return to the launcher.", ATTR_NORMAL);
}

static void draw_programs(void) {
    text(22, 5, "Programs", ATTR_STATUS);
    text(22, 7, "BUILT-IN PROGRAMS", ATTR_NORMAL);
    text(22, 8, "Terminal   interactive command view", ATTR_NORMAL);
    text(22, 9, "Files      read-only filesystem view", ATTR_NORMAL);
    text(22, 10, "Programs   application inventory", ATTR_NORMAL);
    text(22, 11, "About      OS/runtime information", ATTR_NORMAL);
    text(22, 13, "Press Q to return to the launcher.", ATTR_NORMAL);
}

static void draw_about(void) {
    text(22, 5, "About AsterOS", ATTR_STATUS);
    text(22, 7, "32-bit x86 experimental operating system", ATTR_NORMAL);
    text(22, 8, "GUI shell over the CCP/BDOS direction", ATTR_NORMAL);
    text(22, 10, "Enter opens the selected app.", ATTR_NORMAL);
    text(22, 11, "Q returns to the launcher from any app.", ATTR_NORMAL);
}

static void draw_selected_app(void) {
    fill(21, 4, 57, 18, ' ', ATTR_NORMAL);
    if (active_app < 0) {
        text(22, 5, "Select an application", ATTR_STATUS);
        text(22, 7, "Use W/S (or arrow keys) and press Enter.", ATTR_NORMAL);
    } else if (active_app == 0) draw_terminal();
    else if (active_app == 1) draw_files();
    else if (active_app == 2) draw_programs();
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
    text(2, 23, active_app < 0 ? "Enter Open   W/S Navigate" : "Q Launcher   Enter/keys interact", ATTR_STATUS);
}

void gui_init(void) {
    selected = 0;
    active_app = -1;
    terminal_page = 0;
    gui_draw();
}

void gui_handle_key(char key) {
    if (active_app < 0) {
        if (key == 'w' || key == 'W') {
            if (selected == 0) selected = 3; else --selected;
            gui_draw();
        } else if (key == 's' || key == 'S') {
            selected = (uint8_t)((selected + 1) % 4);
            gui_draw();
        } else if (key == '\r' || key == '\n') {
            active_app = (int8_t)selected;
            terminal_page = 0;
            gui_draw();
        }
        return;
    }

    if (key == 'q' || key == 'Q') {
        active_app = -1;
        gui_draw();
    } else if (active_app == 0 && (key == 'h' || key == 'H')) {
        terminal_page = 1;
        gui_draw();
    } else if (active_app == 0 && (key == 'a' || key == 'A')) {
        terminal_page = 2;
        gui_draw();
    } else if (active_app == 0 && (key == 'c' || key == 'C')) {
        terminal_page = 0;
        gui_draw();
    }
}

#ifndef ASTEROS_GUI_H
#define ASTEROS_GUI_H

#include <stdint.h>

void gui_init(void);
void gui_draw(void);
void gui_handle_key(char key);
void gui_handle_mouse(int8_t dx,int8_t dy,uint8_t buttons);

#ifdef ASTEROS_GUI_TEST
extern volatile uint16_t asteros_gui_test_vga[80u * 25u];
#endif

#endif

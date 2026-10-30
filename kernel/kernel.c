#include <stdint.h>
#include "gui.h"
#include "keyboard.h"
#include "ps2_wait.h"
enum { SVC_CONSOLE = 1, SVC_FILES = 2, SVC_PROCESS = 3 };
#define PS2_WAIT_ATTEMPTS 100000u
static uint8_t inb(uint16_t port) { uint8_t value; __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port)); return value; }
static void outb(uint16_t port,uint8_t value) { __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port)); }
static uint8_t controller_status(void *context) {
    (void)context;
    return inb(0x64);
}
static uint8_t wait_controller_input_empty(void) {
    return asteros_ps2_wait_status(controller_status, 0, 0x02, 0x00,
                                    PS2_WAIT_ATTEMPTS);
}
static uint8_t wait_output_full(void) {
    return asteros_ps2_wait_status(controller_status, 0, 0x01, 0x01,
                                    PS2_WAIT_ATTEMPTS);
}
static uint8_t mouse_write(uint8_t value) {
    if (!wait_controller_input_empty()) return 0;
    outb(0x64,0xD4);
    if (!wait_controller_input_empty()) return 0;
    outb(0x60,value);
    return 1;
}
static uint8_t mouse_init(void) {
    uint8_t command;

    /* A missing or stuck auxiliary controller must not prevent keyboard boot. */
    if (!wait_controller_input_empty()) return 0;
    outb(0x64,0xA8);
    if (!wait_controller_input_empty()) return 0;
    outb(0x64,0x20);
    if (!wait_output_full()) return 0;
    command=inb(0x60);
    command=(uint8_t)(command|0x02);
    if (!wait_controller_input_empty()) return 0;
    outb(0x64,0x60);
    if (!wait_controller_input_empty()) return 0;
    outb(0x60,command);
    if (!mouse_write(0xF4)) return 0;
    if (!wait_output_full()) return 0;
    return (uint8_t)(inb(0x60)==0xFA);
}
static uint8_t keyboard_poll(char *key) {
    uint8_t status = inb(0x64);
    if (!(status & 1) || (status & 0x20)) return 0;
    return keyboard_decode_scancode(inb(0x60), key);
}
static uint8_t mouse_poll(int8_t *dx,int8_t *dy,uint8_t *buttons) {
    static uint8_t packet[3];
    static uint8_t index;
    uint8_t status;
    uint8_t value;
    for(;;){
        status=inb(0x64);
        if(!(status&1))return 0;
        if(!(status&0x20))return 0;
        value=inb(0x60);
        if(index==0&&!(value&0x08))continue;
        packet[index++]=value;
        if(index<3)continue;
        index=0;
        if(packet[0]&0xC0)continue;
        *buttons=(uint8_t)(packet[0]&0x07);
        *dx=(int8_t)packet[1];
        *dy=(int8_t)packet[2];
        return 1;
    }
}
void kmain(void){
    char key;
    int8_t dx,dy;
    uint8_t buttons;
    uint8_t mouse_available;
    gui_init();
    mouse_available=mouse_init();
    keyboard_reset_state();
    for(;;){
        /* Keep draining auxiliary bytes after a failed init, but only deliver
           pointer events when the device acknowledged streaming mode. */
        if(mouse_poll(&dx,&dy,&buttons)&&mouse_available)
            gui_handle_mouse(dx,dy,buttons);
        if(keyboard_poll(&key))gui_handle_key(key);
    }
}

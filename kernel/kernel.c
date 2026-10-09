#include <stdint.h>
#include "gui.h"
#include "keyboard.h"
enum { SVC_CONSOLE = 1, SVC_FILES = 2, SVC_PROCESS = 3 };
static uint8_t inb(uint16_t port) { uint8_t value; __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port)); return value; }
static void outb(uint16_t port,uint8_t value) { __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port)); }
static void wait_controller(void) { while(inb(0x64)&2){} }
static void mouse_write(uint8_t value) { wait_controller(); outb(0x64,0xD4); wait_controller(); outb(0x60,value); }
static void mouse_init(void) {
    uint8_t command;
    wait_controller();
    outb(0x64,0xA8);
    wait_controller();
    outb(0x64,0x20);
    while(!(inb(0x64)&1)){}
    command=inb(0x60);
    command=(uint8_t)(command|0x02);
    wait_controller();
    outb(0x64,0x60);
    wait_controller();
    outb(0x60,command);
    mouse_write(0xF4);
    while(!(inb(0x64)&1)){}
    (void)inb(0x60);
}
static uint8_t keyboard_poll(char *key) {
    uint8_t status = inb(0x64);
    if (!(status & 1) || (status & 0x20)) return 0;
    return keyboard_decode(inb(0x60), key);
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
    gui_init();
    mouse_init();
    for(;;){
        if(mouse_poll(&dx,&dy,&buttons))gui_handle_mouse(dx,dy,buttons);
        if(keyboard_poll(&key))gui_handle_key(key);
    }
}

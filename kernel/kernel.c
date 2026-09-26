#include <stdint.h>
#include "gui.h"
enum { SVC_CONSOLE = 1, SVC_FILES = 2, SVC_PROCESS = 3 };
enum { KEY_UP = 0x80, KEY_DOWN = 0x81 };
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
    static uint8_t extended;
    uint8_t status;
    uint8_t scancode;
    status=inb(0x64);
    if(!(status&1)||status&0x20)return 0;
    scancode=inb(0x60);
    if(scancode==0xE0){extended=1;return 0;}
    if(scancode&0x80){extended=0;return 0;}
    if(extended){
        extended=0;
        if(scancode==0x48){*key=(char)KEY_UP;return 1;}
        if(scancode==0x50){*key=(char)KEY_DOWN;return 1;}
        return 0;
    }
    switch(scancode) {
        case 0x1C:*key='\r';return 1; case 0x0E:*key='\b';return 1; case 0x39:*key=' ';return 1;
        case 0x10:*key='q';return 1; case 0x11:*key='w';return 1; case 0x12:*key='e';return 1; case 0x13:*key='r';return 1;
        case 0x14:*key='t';return 1; case 0x15:*key='y';return 1; case 0x16:*key='u';return 1; case 0x17:*key='i';return 1;
        case 0x18:*key='o';return 1; case 0x19:*key='p';return 1; case 0x1E:*key='a';return 1; case 0x1F:*key='s';return 1;
        case 0x20:*key='d';return 1; case 0x21:*key='f';return 1; case 0x22:*key='g';return 1; case 0x23:*key='h';return 1;
        case 0x24:*key='j';return 1; case 0x25:*key='k';return 1; case 0x26:*key='l';return 1; case 0x2C:*key='z';return 1;
        case 0x2D:*key='x';return 1; case 0x2E:*key='c';return 1; case 0x2F:*key='v';return 1; case 0x30:*key='b';return 1;
        case 0x31:*key='n';return 1; case 0x32:*key='m';return 1; case 0x02:*key='1';return 1; case 0x03:*key='2';return 1;
        case 0x04:*key='3';return 1; case 0x05:*key='4';return 1; case 0x06:*key='5';return 1; case 0x07:*key='6';return 1;
        case 0x08:*key='7';return 1; case 0x09:*key='8';return 1; case 0x0A:*key='9';return 1; case 0x0B:*key='0';return 1;
        default:return 0;
    }
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

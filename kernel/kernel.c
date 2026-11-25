#include <stdint.h>
#include "gui.h"
#include "desktop_input.h"
#include "keyboard.h"
#include "ps2_wait.h"
#include "net.h"
#include "arp.h"
#include "ipv4.h"
#include "icmp.h"
#include "pci.h"
#include "ne2k.h"
#include "fs.h"
#include "ata_pio.h"
#include "volume.h"
enum { SVC_CONSOLE = 1, SVC_FILES = 2, SVC_PROCESS = 3 };
#define PS2_WAIT_ATTEMPTS 100000u
static uint8_t inb(uint16_t port) { uint8_t value; __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port)); return value; }
static void outb(uint16_t port,uint8_t value) { __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port)); }
static uint16_t inw(uint16_t port) { uint16_t value; __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port)); return value; }
static void outw(uint16_t port,uint16_t value) { __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port)); }
static uint8_t ata_in8(void *context,uint16_t port) { (void)context; return inb(port); }
static uint16_t ata_in16(void *context,uint16_t port) { (void)context; return inw(port); }
static void ata_out8(void *context,uint16_t port,uint8_t value) { (void)context; outb(port,value); }
static void ata_out16(void *context,uint16_t port,uint16_t value) { (void)context; outw(port,value); }
static const asteros_ata_io_t ata_io = {
    0, ata_in8, ata_in16, ata_out8, ata_out16
};
static asteros_ata_device_t ata_primary_master;
static asteros_volume_t primary_asteros_volume;
static uint8_t ata_read_sector(void *context, uint32_t lba, uint8_t *sector) {
    const asteros_ata_device_t *device = (const asteros_ata_device_t *)context;
    return (uint8_t)(asteros_ata_pio_read(device, lba, sector) == ASTEROS_ATA_OK);
}
/* RFC 5737 documentation address reserved for the initial ARP peer test. */
static const uint8_t network_ipv4_address[4] = {192u, 0u, 2u, 2u};

/* Use the PIT as a polled monotonic clock; the kernel currently has no IRQ
   timer path. Channel 0 runs at the standard 1.193182 MHz input clock. */
static uint16_t timer_last_count;
static uint32_t timer_elapsed_ticks;
static uint16_t timer_read_counter(void) {
    uint8_t low;
    uint8_t high;
    outb(0x43, 0x00); /* Latch channel 0's current count. */
    low = inb(0x40);
    high = inb(0x40);
    return (uint16_t)((uint16_t)low | ((uint16_t)high << 8));
}
static void timer_init(void) {
    outb(0x43, 0x34); /* Channel 0, lo/hi access, rate generator, binary. */
    outb(0x40, 0xFF);
    outb(0x40, 0xFF);
    timer_last_count = timer_read_counter();
    timer_elapsed_ticks = 0;
}
static uint32_t timer_now(void) {
    uint16_t current = timer_read_counter();
    timer_elapsed_ticks += (uint16_t)(timer_last_count - current);
    timer_last_count = current;
    return timer_elapsed_ticks;
}
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
    uint8_t mouse_buttons = 0;
    asteros_mouse_click_filter_t click_filter = {0, 0, 0};
    asteros_net_init();
    asteros_pci_init();
    asteros_ne2k_init_default();
    (void)asteros_arp_init();
    (void)asteros_ipv4_init();
    (void)asteros_icmp_init();
    if (asteros_netif_count() != 0u) {
        (void)asteros_arp_configure(asteros_netif_at(0),
                                    network_ipv4_address);
        (void)asteros_ipv4_configure(asteros_netif_at(0),
                                     network_ipv4_address);
    }
    asteros_fs_init();
    /* Probe only: no sector is written until a validated AsterOS volume exists. */
    (void)asteros_ata_pio_probe(&ata_primary_master, &ata_io,
                                0x1F0u, 0x3F6u, 0u);
    if (ata_primary_master.present) {
        (void)asteros_volume_probe(ata_read_sector, &ata_primary_master,
                                   ata_primary_master.sectors,
                                   &primary_asteros_volume);
    }
    gui_init();
    gui_set_ata_disk_present(ata_primary_master.present);
    gui_set_asteros_volume_present(primary_asteros_volume.present);
    mouse_available=mouse_init();
    timer_init();
    keyboard_reset_state();
    for(;;){
        asteros_ne2k_poll_default();
        /* Keep draining auxiliary bytes after a failed init, but only deliver
           pointer events when the device acknowledged streaming mode. */
        if(mouse_poll(&dx,&dy,&buttons)&&mouse_available){
            mouse_buttons=buttons;
            /* Left clicks are delivered by the timed filter below. Keep
               pointer motion and right/middle buttons responsive immediately. */
            gui_handle_mouse(dx,dy,(uint8_t)(buttons&~1u));
        }
        if(mouse_available&&asteros_mouse_click_filter_update(
            &click_filter,(uint8_t)(mouse_buttons&1u),timer_now())){
            gui_handle_mouse(0,0,1);
            gui_handle_mouse(0,0,0);
        }
        if(keyboard_poll(&key))gui_handle_key(key);
    }
}

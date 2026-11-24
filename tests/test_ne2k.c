#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ne2k.h"

#define FAKE_MEMORY_SIZE 65536u
#define TEST_BASE 0xC000u
#define TEST_ETHERTYPE 0x88B5u

typedef struct {
    uint16_t base;
    uint8_t page0[16];
    uint8_t page1[16];
    uint8_t memory[FAKE_MEMORY_SIZE];
    uint8_t command;
    uint8_t isr;
    uint16_t dma_address;
    uint16_t dma_remaining;
    uint8_t reset_ack;
    uint8_t dma_ack;
    uint8_t transmit_ack;
    uint8_t enable_ack;
    uint8_t enable_calls;
    uint16_t last_transmit_length;
} fake_ne2k_t;

typedef struct {
    uint8_t calls;
    uint16_t length;
    uint8_t prefix[16];
} received_frame_t;

static const uint8_t test_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};

static uint8_t fake_page(const fake_ne2k_t *fake) {
    return (uint8_t)((fake->command >> 6) & 0x03u);
}

static uint8_t fake_read8(void *context, uint16_t port) {
    fake_ne2k_t *fake = (fake_ne2k_t *)context;
    uint16_t offset = (uint16_t)(port - fake->base);

    if (offset == 0x1Fu) return 0u;
    if (offset == 0x07u && fake_page(fake) == 1u)
        return fake->page1[7];
    if (offset == 0x07u) return fake->isr;
    if (offset >= 16u) return 0xFFu;
    return fake_page(fake) == 1u
        ? fake->page1[offset] : fake->page0[offset];
}

static void fake_begin_dma(fake_ne2k_t *fake, uint8_t command) {
    uint8_t mode = (uint8_t)((command >> 3) & 0x07u);

    if (mode != 1u && mode != 2u) return;
    fake->dma_address = (uint16_t)(
        (uint16_t)fake->page0[0x08] |
        ((uint16_t)fake->page0[0x09] << 8));
    fake->dma_remaining = (uint16_t)(
        (uint16_t)fake->page0[0x0A] |
        ((uint16_t)fake->page0[0x0B] << 8));
    if (fake->dma_remaining == 0u && fake->dma_ack)
        fake->isr |= 0x40u;
}

static void fake_write8(void *context, uint16_t port, uint8_t value) {
    fake_ne2k_t *fake = (fake_ne2k_t *)context;
    uint16_t offset = (uint16_t)(port - fake->base);

    if (offset == 0x1Fu) {
        if (fake->reset_ack) fake->isr |= 0x80u;
        return;
    }
    if (offset == 0u) {
        fake->command = value;
        if (!(value & 0x01u)) fake->isr &= (uint8_t)~0x80u;
        fake_begin_dma(fake, value);
        if ((value & 0x04u) && fake->transmit_ack) {
            fake->isr |= 0x02u;
            fake->last_transmit_length = (uint16_t)(
                (uint16_t)fake->page0[0x05] |
                ((uint16_t)fake->page0[0x06] << 8));
        }
        return;
    }
    if (offset == 0x07u && fake_page(fake) != 1u) {
        fake->isr = (uint8_t)(fake->isr & (uint8_t)~(value & 0x7Fu));
        return;
    }
    if (offset >= 16u) return;
    if (fake_page(fake) == 1u) fake->page1[offset] = value;
    else fake->page0[offset] = value;
}

static void fake_advance_dma(fake_ne2k_t *fake) {
    if (fake->dma_remaining >= 2u) {
        fake->dma_address = (uint16_t)(fake->dma_address + 2u);
        fake->dma_remaining = (uint16_t)(fake->dma_remaining - 2u);
        if (fake->dma_remaining == 0u && fake->dma_ack)
            fake->isr |= 0x40u;
    }
}

static uint16_t fake_read16(void *context, uint16_t port) {
    fake_ne2k_t *fake = (fake_ne2k_t *)context;
    uint16_t offset = (uint16_t)(port - fake->base);
    uint16_t value;

    assert(offset == 0x10u);
    value = (uint16_t)(
        (uint16_t)fake->memory[fake->dma_address] |
        ((uint16_t)fake->memory[(uint16_t)(fake->dma_address + 1u)] << 8));
    fake_advance_dma(fake);
    return value;
}

static void fake_write16(void *context, uint16_t port, uint16_t value) {
    fake_ne2k_t *fake = (fake_ne2k_t *)context;
    uint16_t offset = (uint16_t)(port - fake->base);

    assert(offset == 0x10u);
    fake->memory[fake->dma_address] = (uint8_t)value;
    fake->memory[(uint16_t)(fake->dma_address + 1u)] =
        (uint8_t)(value >> 8);
    fake_advance_dma(fake);
}

static uint8_t fake_enable_io(void *context,
                              const asteros_pci_device_t *device) {
    fake_ne2k_t *fake = (fake_ne2k_t *)context;
    assert(device->vendor_id == ASTEROS_NE2K_VENDOR_REALTEK);
    assert(device->device_id == ASTEROS_NE2K_DEVICE_RTL8029);
    fake->enable_calls++;
    return fake->enable_ack;
}

static const asteros_ne2k_ops_t fake_ops = {
    fake_read8,
    fake_write8,
    fake_read16,
    fake_write16,
    fake_enable_io
};

static void fake_init(fake_ne2k_t *fake) {
    uint8_t index;

    memset(fake, 0, sizeof *fake);
    fake->base = TEST_BASE;
    fake->reset_ack = 1u;
    fake->dma_ack = 1u;
    fake->transmit_ack = 1u;
    fake->enable_ack = 1u;
    for (index = 0; index < 6u; index++) {
        fake->memory[index * 2u] = test_mac[index];
        fake->memory[index * 2u + 1u] = test_mac[index];
    }
}

static void make_device(asteros_pci_device_t *device) {
    memset(device, 0, sizeof *device);
    device->vendor_id = ASTEROS_NE2K_VENDOR_REALTEK;
    device->device_id = ASTEROS_NE2K_DEVICE_RTL8029;
    device->base_class = 0x02u;
    device->subclass = 0x00u;
    device->header_type = 0u;
}

static void make_resources(asteros_pci_resources_t *resources) {
    memset(resources, 0, sizeof *resources);
    resources->slot_count = 6u;
    resources->count = 1u;
    resources->bars[0].index = 0u;
    resources->bars[0].type = ASTEROS_PCI_BAR_IO;
    resources->bars[0].width = 32u;
    resources->bars[0].base_address = TEST_BASE;
}

static asteros_ne2k_result_t start_driver(
    asteros_ne2k_driver_t *driver, fake_ne2k_t *fake,
    asteros_pci_device_t *device, asteros_pci_resources_t *resources) {
    return asteros_ne2k_start(driver, device, resources, &fake_ops, fake);
}

static void test_device_and_bar_validation_precedes_enable(void) {
    fake_ne2k_t fake;
    asteros_ne2k_driver_t driver;
    asteros_pci_device_t device;
    asteros_pci_resources_t resources;

    fake_init(&fake);
    make_device(&device);
    make_resources(&resources);
    device.device_id++;
    assert(start_driver(&driver, &fake, &device, &resources) ==
           ASTEROS_NE2K_UNSUPPORTED_DEVICE);
    assert(fake.enable_calls == 0u);

    make_device(&device);
    resources.bars[0].type = ASTEROS_PCI_BAR_MEMORY;
    assert(start_driver(&driver, &fake, &device, &resources) ==
           ASTEROS_NE2K_INVALID_BAR);
    assert(fake.enable_calls == 0u);

    make_resources(&resources);
    resources.bars[0].base_address = 0xC010u;
    assert(start_driver(&driver, &fake, &device, &resources) ==
           ASTEROS_NE2K_INVALID_BAR);
    assert(fake.enable_calls == 0u);

    make_resources(&resources);
    resources.bars[0].base_address = 0xFFF0u;
    assert(start_driver(&driver, &fake, &device, &resources) ==
           ASTEROS_NE2K_INVALID_BAR);
    assert(fake.enable_calls == 0u);

    make_resources(&resources);
    resources.bars[0].width = 64u;
    assert(start_driver(&driver, &fake, &device, &resources) ==
           ASTEROS_NE2K_INVALID_BAR);
    assert(fake.enable_calls == 0u);
}

static void test_pci_enable_failure_does_not_touch_the_device(void) {
    fake_ne2k_t fake;
    asteros_ne2k_driver_t driver;
    asteros_pci_device_t device;
    asteros_pci_resources_t resources;

    fake_init(&fake);
    fake.enable_ack = 0u;
    make_device(&device);
    make_resources(&resources);
    assert(start_driver(&driver, &fake, &device, &resources) ==
           ASTEROS_NE2K_PCI_ENABLE_FAILED);
    assert(fake.enable_calls == 1u);
    assert(fake.command == 0u);
    assert(asteros_netif_count() == 0u);
}

static void test_initialization_and_transmit(void) {
    fake_ne2k_t fake;
    asteros_ne2k_driver_t driver;
    asteros_pci_device_t device;
    asteros_pci_resources_t resources;
    uint8_t frame[ASTEROS_ETHERNET_HEADER_SIZE];
    uint8_t index;

    fake_init(&fake);
    make_device(&device);
    make_resources(&resources);
    asteros_net_init();
    assert(start_driver(&driver, &fake, &device, &resources) ==
           ASTEROS_NE2K_OK);
    assert(fake.enable_calls == 1u);
    assert(driver.initialized == 1u);
    assert(driver.io_base == TEST_BASE);
    assert(asteros_netif_count() == 1u);
    assert(asteros_netif_at(0) == &driver.interface);
    assert(strcmp(driver.interface.name, "eth0") == 0);
    assert(memcmp(driver.interface.mac, test_mac, sizeof test_mac) == 0);
    assert(driver.interface.link_up == 1u);
    assert(fake.page0[0x0E] == 0x49u);
    assert(fake.page0[0x0C] == 0x04u);
    assert(fake.page0[0x0F] == 0u);
    assert(fake.page0[0x01] == 0x46u);
    assert(fake.page0[0x02] == 0x80u);
    assert(fake.page0[0x03] == 0x45u);
    assert(fake.page1[0x07] == 0x46u);

    memset(frame, 0, sizeof frame);
    frame[0] = 0xFFu;
    frame[6] = test_mac[0];
    frame[7] = test_mac[1];
    frame[8] = test_mac[2];
    frame[9] = test_mac[3];
    frame[10] = test_mac[4];
    frame[11] = test_mac[5];
    frame[12] = 0x88u;
    frame[13] = 0xB5u;
    assert(asteros_netif_send_frame(&driver.interface, frame,
                                    sizeof frame) == ASTEROS_NET_OK);
    assert(fake.last_transmit_length == 60u);
    assert(memcmp(&fake.memory[0x4000u], frame, sizeof frame) == 0);
    for (index = (uint8_t)sizeof frame; index < 60u; index++)
        assert(fake.memory[0x4000u + index] == 0u);

    asteros_ne2k_stop(&driver);
    assert(driver.initialized == 0u);
    assert(driver.interface.link_up == 0u);
}

static void capture_frame(asteros_netif_t *interface,
                          const uint8_t *source_mac,
                          const uint8_t *destination_mac,
                          const uint8_t *payload, uint16_t length,
                          void *context) {
    received_frame_t *received = (received_frame_t *)context;
    uint16_t index;
    (void)interface;
    (void)source_mac;
    (void)destination_mac;

    received->calls++;
    received->length = length;
    for (index = 0; index < sizeof received->prefix && index < length; index++)
        received->prefix[index] = payload[index];
}

static void inject_packet(fake_ne2k_t *fake, uint8_t page, uint8_t status,
                          uint8_t next_page, const uint8_t *frame,
                          uint16_t frame_length) {
    uint16_t count = (uint16_t)(frame_length + 4u);
    uint16_t total = (uint16_t)(count + 4u);
    uint16_t index;
    uint16_t address = (uint16_t)(((uint16_t)page << 8) + 4u);
    uint16_t ring_end = (uint16_t)(0x80u << 8);

    fake->memory[(uint16_t)(page << 8)] = status;
    fake->memory[(uint16_t)((page << 8) + 1u)] = next_page;
    fake->memory[(uint16_t)((page << 8) + 2u)] = (uint8_t)count;
    fake->memory[(uint16_t)((page << 8) + 3u)] = (uint8_t)(count >> 8);
    for (index = 0; index < frame_length; index++) {
        fake->memory[address] = frame[index];
        address++;
        if (address >= ring_end) address = (uint16_t)(0x46u << 8);
    }
    for (index = 0; index < 4u; index++) {
        fake->memory[address] = 0u; /* Ethernet FCS is not delivered upward. */
        address++;
        if (address >= ring_end) address = (uint16_t)(0x46u << 8);
    }
    assert(total != 0u);
    fake->isr |= 0x01u;
}

static void test_receive_dispatch_and_ring_wrap(void) {
    fake_ne2k_t fake;
    asteros_ne2k_driver_t driver;
    asteros_pci_device_t device;
    asteros_pci_resources_t resources;
    received_frame_t received;
    uint8_t frame[300];
    uint16_t index;
    uint8_t next_page;

    fake_init(&fake);
    make_device(&device);
    make_resources(&resources);
    memset(&received, 0, sizeof received);
    for (index = 0; index < sizeof frame; index++)
        frame[index] = (uint8_t)(index ^ 0x5Au);
    frame[12] = (uint8_t)(TEST_ETHERTYPE >> 8);
    frame[13] = (uint8_t)TEST_ETHERTYPE;

    asteros_net_init();
    assert(asteros_net_register_ethertype(TEST_ETHERTYPE, capture_frame,
                                           &received) == ASTEROS_NET_OK);
    assert(start_driver(&driver, &fake, &device, &resources) ==
           ASTEROS_NE2K_OK);
    assert(asteros_ne2k_poll(&driver) == ASTEROS_NE2K_POLL_NONE);

    /* A 300-byte frame at page 0x7F crosses PSTOP and resumes at PSTART. */
    next_page = 0x47u;
    fake.page0[0x03] = 0x7Eu;
    fake.page1[0x07] = next_page;
    inject_packet(&fake, 0x7Fu, 0x01u, next_page, frame, sizeof frame);
    assert(asteros_ne2k_poll(&driver) == ASTEROS_NE2K_POLL_FRAME);
    assert(received.calls == 1u);
    assert(received.length == sizeof frame - ASTEROS_ETHERNET_HEADER_SIZE);
    assert(memcmp(received.prefix, frame + ASTEROS_ETHERNET_HEADER_SIZE,
                  sizeof received.prefix) == 0);
    assert(fake.page0[0x03] == 0x46u);
    assert(asteros_ne2k_poll(&driver) == ASTEROS_NE2K_POLL_NONE);
    assert((fake.isr & 0x01u) == 0u);
}

static void test_malformed_ring_header_is_dropped_safely(void) {
    fake_ne2k_t fake;
    asteros_ne2k_driver_t driver;
    asteros_pci_device_t device;
    asteros_pci_resources_t resources;
    received_frame_t received;
    uint8_t frame[64];
    uint16_t index;

    fake_init(&fake);
    make_device(&device);
    make_resources(&resources);
    memset(&received, 0, sizeof received);
    memset(frame, 0xA5, sizeof frame);
    frame[12] = (uint8_t)(TEST_ETHERTYPE >> 8);
    frame[13] = (uint8_t)TEST_ETHERTYPE;

    asteros_net_init();
    assert(asteros_net_register_ethertype(TEST_ETHERTYPE, capture_frame,
                                           &received) == ASTEROS_NET_OK);
    assert(start_driver(&driver, &fake, &device, &resources) ==
           ASTEROS_NE2K_OK);

    fake.page1[0x07] = 0x47u;
    inject_packet(&fake, 0x46u, 0x01u, 0x48u, frame, sizeof frame);
    assert(asteros_ne2k_poll(&driver) == ASTEROS_NE2K_POLL_DROPPED);
    assert(received.calls == 0u);
    assert(fake.page0[0x03] == 0x46u);

    /* A valid ring pointer with a bad receive status is also consumed. */
    fake.page0[0x03] = 0x45u;
    inject_packet(&fake, 0x46u, 0x03u, 0x47u, frame, sizeof frame);
    assert(asteros_ne2k_poll(&driver) == ASTEROS_NE2K_POLL_DROPPED);
    assert(received.calls == 0u);
    assert(fake.page0[0x03] == 0x46u);
    for (index = 0; index < 6u; index++)
        assert(driver.interface.mac[index] == test_mac[index]);
}

static void test_reset_and_dma_timeouts_are_bounded(void) {
    fake_ne2k_t fake;
    asteros_ne2k_driver_t driver;
    asteros_pci_device_t device;
    asteros_pci_resources_t resources;

    fake_init(&fake);
    fake.reset_ack = 0u;
    make_device(&device);
    make_resources(&resources);
    asteros_net_init();
    assert(start_driver(&driver, &fake, &device, &resources) ==
           ASTEROS_NE2K_RESET_TIMEOUT);
    assert(asteros_netif_count() == 0u);

    fake_init(&fake);
    fake.dma_ack = 0u;
    asteros_net_init();
    assert(start_driver(&driver, &fake, &device, &resources) ==
           ASTEROS_NE2K_DMA_TIMEOUT);
    assert(asteros_netif_count() == 0u);
}

int main(void) {
    test_device_and_bar_validation_precedes_enable();
    test_pci_enable_failure_does_not_touch_the_device();
    test_initialization_and_transmit();
    test_receive_dispatch_and_ring_wrap();
    test_malformed_ring_header_is_dropped_safely();
    test_reset_and_dma_timeouts_are_bounded();
    puts("NE2K driver: tests passed");
    return 0;
}

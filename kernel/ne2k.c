#include "ne2k.h"

#define NE2K_WAIT_ATTEMPTS 100000u
#define NE2K_REG_COMMAND 0x00u
#define NE2K_REG_PSTART 0x01u
#define NE2K_REG_PSTOP 0x02u
#define NE2K_REG_BOUNDARY 0x03u
#define NE2K_REG_TX_PAGE 0x04u
#define NE2K_REG_TX_COUNT0 0x05u
#define NE2K_REG_TX_COUNT1 0x06u
#define NE2K_REG_ISR 0x07u
#define NE2K_REG_RSAR0 0x08u
#define NE2K_REG_RSAR1 0x09u
#define NE2K_REG_RBCR0 0x0Au
#define NE2K_REG_RBCR1 0x0Bu
#define NE2K_REG_RCR 0x0Cu
#define NE2K_REG_TCR 0x0Du
#define NE2K_REG_DCR 0x0Eu
#define NE2K_REG_IMR 0x0Fu
#define NE2K_REG_DATA 0x10u
#define NE2K_REG_RESET 0x1Fu
#define NE2K_ISR_PRX 0x01u
#define NE2K_ISR_PTX 0x02u
#define NE2K_ISR_TXE 0x08u
#define NE2K_ISR_OVW 0x10u
#define NE2K_ISR_RDC 0x40u
#define NE2K_ISR_RST 0x80u
#define NE2K_COMMAND_STOP_ABORT 0x21u
#define NE2K_COMMAND_START_ABORT 0x22u
#define NE2K_COMMAND_START_READ 0x0Au
#define NE2K_COMMAND_START_WRITE 0x12u
#define NE2K_COMMAND_START_TX 0x26u
#define NE2K_COMMAND_PAGE1 0x62u
#define NE2K_TX_PAGE 0x40u
#define NE2K_RX_START 0x46u
#define NE2K_RX_STOP 0x80u
#define NE2K_TX_MINIMUM 60u
#define NE2K_RX_STATUS_GOOD 0x01u
#define NE2K_RX_COUNT_MIN 18u
#define NE2K_RX_COUNT_MAX (ASTEROS_NE2K_FRAME_MAX + 4u)

static uint8_t reg_read(asteros_ne2k_driver_t *driver, uint8_t reg) {
    return driver->ops.read8(driver->io_context,
                             (uint16_t)(driver->io_base + reg));
}

static void reg_write(asteros_ne2k_driver_t *driver, uint8_t reg,
                      uint8_t value) {
    driver->ops.write8(driver->io_context,
                       (uint16_t)(driver->io_base + reg), value);
}

static uint8_t wait_isr(asteros_ne2k_driver_t *driver, uint8_t mask,
                        uint8_t *observed) {
    uint32_t attempt;

    if (observed) *observed = 0;
    for (attempt = 0; attempt < NE2K_WAIT_ATTEMPTS; attempt++) {
        uint8_t status = reg_read(driver, NE2K_REG_ISR);
        if (status & mask) {
            if (observed) *observed = status;
            return 1;
        }
    }
    return 0;
}

/* The RTL8029 is a 16-bit NE2000-compatible adapter. Round byte counts up
   to a word because the data port is accessed with INW/OUTW. The caller only
   consumes the requested bytes; the extra read byte is harmless padding. */
static uint8_t dma_read(asteros_ne2k_driver_t *driver, uint16_t address,
                        uint8_t *destination, uint16_t length) {
    uint16_t transfer_length;
    uint16_t offset;

    if (!destination || length == 0u) return 0;
    transfer_length = (uint16_t)(length + (length & 1u));
    reg_write(driver, NE2K_REG_RSAR0, (uint8_t)address);
    reg_write(driver, NE2K_REG_RSAR1, (uint8_t)(address >> 8));
    reg_write(driver, NE2K_REG_RBCR0, (uint8_t)transfer_length);
    reg_write(driver, NE2K_REG_RBCR1, (uint8_t)(transfer_length >> 8));
    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_START_READ);

    for (offset = 0; offset < transfer_length; offset = (uint16_t)(offset + 2u)) {
        uint16_t word = driver->ops.read16(
            driver->io_context, (uint16_t)(driver->io_base + NE2K_REG_DATA));
        if (offset < length) destination[offset] = (uint8_t)word;
        if ((uint16_t)(offset + 1u) < length)
            destination[offset + 1u] = (uint8_t)(word >> 8);
    }

    if (!wait_isr(driver, NE2K_ISR_RDC, 0)) {
        reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_START_ABORT);
        return 0;
    }
    reg_write(driver, NE2K_REG_ISR, NE2K_ISR_RDC);
    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_START_ABORT);
    return 1;
}

static uint8_t dma_write(asteros_ne2k_driver_t *driver, uint16_t address,
                         const uint8_t *source, uint16_t length) {
    uint16_t transfer_length;
    uint16_t offset;

    if (!source || length == 0u) return 0;
    transfer_length = (uint16_t)(length + (length & 1u));
    reg_write(driver, NE2K_REG_RSAR0, (uint8_t)address);
    reg_write(driver, NE2K_REG_RSAR1, (uint8_t)(address >> 8));
    reg_write(driver, NE2K_REG_RBCR0, (uint8_t)transfer_length);
    reg_write(driver, NE2K_REG_RBCR1, (uint8_t)(transfer_length >> 8));
    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_START_WRITE);

    for (offset = 0; offset < transfer_length; offset = (uint16_t)(offset + 2u)) {
        uint16_t word = source[offset];
        if ((uint16_t)(offset + 1u) < length)
            word |= (uint16_t)((uint16_t)source[offset + 1u] << 8);
        driver->ops.write16(driver->io_context,
                            (uint16_t)(driver->io_base + NE2K_REG_DATA), word);
    }

    if (!wait_isr(driver, NE2K_ISR_RDC, 0)) {
        reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_START_ABORT);
        return 0;
    }
    reg_write(driver, NE2K_REG_ISR, NE2K_ISR_RDC);
    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_START_ABORT);
    return 1;
}

static uint8_t mac_valid(const uint8_t mac[6]) {
    uint8_t index;
    uint8_t nonzero = 0;

    if (mac[0] & 1u) return 0;
    for (index = 0; index < 6u; index++) {
        if (mac[index]) nonzero = 1;
    }
    return nonzero;
}

static uint8_t reset_chip(asteros_ne2k_driver_t *driver) {
    uint8_t reset_value;

    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_STOP_ABORT);
    reset_value = reg_read(driver, NE2K_REG_RESET);
    driver->ops.write8(driver->io_context,
                       (uint16_t)(driver->io_base + NE2K_REG_RESET),
                       reset_value);
    if (!wait_isr(driver, NE2K_ISR_RST, 0)) return 0;
    reg_write(driver, NE2K_REG_ISR, 0xFFu);
    return 1;
}

static asteros_ne2k_result_t initialize_chip(asteros_ne2k_driver_t *driver,
                                               uint8_t mac[6]) {
    uint8_t prom[32];
    uint8_t index;

    if (!reset_chip(driver)) return ASTEROS_NE2K_RESET_TIMEOUT;

    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_STOP_ABORT);
    reg_write(driver, NE2K_REG_IMR, 0u);       /* Polling; no IRQ is enabled. */
    reg_write(driver, NE2K_REG_DCR, 0x49u);    /* 16-bit DMA, byte order, FIFO. */
    reg_write(driver, NE2K_REG_RBCR0, 0u);
    reg_write(driver, NE2K_REG_RBCR1, 0u);
    reg_write(driver, NE2K_REG_RCR, 0x20u);    /* Monitor while configuring. */
    reg_write(driver, NE2K_REG_TCR, 0x02u);    /* Internal loopback. */
    reg_write(driver, NE2K_REG_PSTART, NE2K_RX_START);
    reg_write(driver, NE2K_REG_PSTOP, NE2K_RX_STOP);
    reg_write(driver, NE2K_REG_BOUNDARY, (uint8_t)(NE2K_RX_START - 1u));
    reg_write(driver, NE2K_REG_TX_PAGE, NE2K_TX_PAGE);
    reg_write(driver, NE2K_REG_TX_COUNT0, 0u);
    reg_write(driver, NE2K_REG_TX_COUNT1, 0u);
    reg_write(driver, NE2K_REG_ISR, 0xFFu);

    /* The 16-bit PROM presents each station-address byte in the low byte of
       a word. Read the full 32-byte PROM and extract the first six words. */
    if (!dma_read(driver, 0u, prom, sizeof prom))
        return ASTEROS_NE2K_DMA_TIMEOUT;
    for (index = 0; index < 6u; index++) mac[index] = prom[index * 2u];
    if (!mac_valid(mac)) return ASTEROS_NE2K_INVALID_MAC;

    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_PAGE1);
    for (index = 0; index < 6u; index++)
        reg_write(driver, (uint8_t)(0x01u + index), mac[index]);
    for (index = 0; index < 8u; index++)
        reg_write(driver, (uint8_t)(0x08u + index), 0u);
    reg_write(driver, 0x07u, NE2K_RX_START); /* CURR on page 1. */

    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_START_ABORT);
    reg_write(driver, NE2K_REG_ISR, 0xFFu);
    reg_write(driver, NE2K_REG_RCR, 0x04u); /* Accept broadcast and our MAC. */
    reg_write(driver, NE2K_REG_TCR, 0u);
    reg_write(driver, NE2K_REG_IMR, 0u);
    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_START_ABORT);
    return ASTEROS_NE2K_OK;
}

static uint8_t ne2k_transmit(void *context, const uint8_t *frame,
                             uint16_t length) {
    asteros_ne2k_driver_t *driver = (asteros_ne2k_driver_t *)context;
    uint16_t transmit_length;
    uint16_t index;
    uint8_t status;

    if (!driver || !driver->initialized || !frame ||
        length < ASTEROS_ETHERNET_HEADER_SIZE ||
        length > ASTEROS_NE2K_FRAME_MAX)
        return 0;

    transmit_length = length < NE2K_TX_MINIMUM ? NE2K_TX_MINIMUM : length;
    for (index = 0; index < length; index++)
        driver->transmit_buffer[index] = frame[index];
    for (; index < transmit_length; index++)
        driver->transmit_buffer[index] = 0u;

    if (!dma_write(driver, (uint16_t)(NE2K_TX_PAGE << 8),
                   driver->transmit_buffer, transmit_length))
        return 0;

    reg_write(driver, NE2K_REG_ISR, (uint8_t)(NE2K_ISR_PTX | NE2K_ISR_TXE));
    reg_write(driver, NE2K_REG_TX_PAGE, NE2K_TX_PAGE);
    reg_write(driver, NE2K_REG_TX_COUNT0, (uint8_t)transmit_length);
    reg_write(driver, NE2K_REG_TX_COUNT1, (uint8_t)(transmit_length >> 8));
    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_START_TX);

    if (!wait_isr(driver, (uint8_t)(NE2K_ISR_PTX | NE2K_ISR_TXE), &status))
        return 0;
    reg_write(driver, NE2K_REG_ISR,
              (uint8_t)(status & (NE2K_ISR_PTX | NE2K_ISR_TXE)));
    return (uint8_t)((status & NE2K_ISR_PTX) != 0u &&
                     (status & NE2K_ISR_TXE) == 0u);
}

static uint8_t find_io_bar(const asteros_pci_resources_t *resources,
                           uint16_t *base) {
    uint8_t index;

    if (!resources || !base || resources->slot_count == 0u ||
        resources->count > ASTEROS_PCI_BAR_MAX)
        return 0;

    for (index = 0; index < resources->count; index++) {
        const asteros_pci_bar_t *bar = &resources->bars[index];
        if (bar->index != 0u) continue;
        if (bar->type != ASTEROS_PCI_BAR_IO || bar->width != 32u ||
            bar->base_address == 0u || bar->base_address > 0xFFE0u ||
            (bar->base_address & 0x1Fu) != 0u)
            return 0;
        *base = (uint16_t)bar->base_address;
        return 1;
    }
    return 0;
}

asteros_ne2k_result_t asteros_ne2k_start(
    asteros_ne2k_driver_t *driver, const asteros_pci_device_t *device,
    const asteros_pci_resources_t *resources,
    const asteros_ne2k_ops_t *ops, void *io_context) {
    uint16_t io_base;
    uint8_t mac[6];
    uint8_t index;
    asteros_ne2k_result_t result;

    if (!driver || !device || !resources || !ops ||
        !ops->read8 || !ops->write8 || !ops->read16 || !ops->write16 ||
        !ops->enable_io)
        return ASTEROS_NE2K_INVALID_ARGUMENT;

    if (device->vendor_id != ASTEROS_NE2K_VENDOR_REALTEK ||
        device->device_id != ASTEROS_NE2K_DEVICE_RTL8029 ||
        device->base_class != 0x02u || device->subclass != 0x00u ||
        device->header_type != 0u)
        return ASTEROS_NE2K_UNSUPPORTED_DEVICE;

    if (!find_io_bar(resources, &io_base))
        return ASTEROS_NE2K_INVALID_BAR;

    driver->ops = *ops;
    driver->io_context = io_context;
    driver->io_base = io_base;
    driver->initialized = 0u;
    if (!driver->ops.enable_io(driver->io_context, device))
        return ASTEROS_NE2K_PCI_ENABLE_FAILED;

    result = initialize_chip(driver, mac);
    if (result != ASTEROS_NE2K_OK) {
        reg_write(driver, NE2K_REG_IMR, 0u);
        reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_STOP_ABORT);
        return result;
    }

    for (index = 0; index < ASTEROS_NETIF_NAME_MAX; index++)
        driver->interface.name[index] = 0;
    driver->interface.name[0] = 'e';
    driver->interface.name[1] = 't';
    driver->interface.name[2] = 'h';
    driver->interface.name[3] = '0';
    for (index = 0; index < 6u; index++)
        driver->interface.mac[index] = mac[index];
    driver->interface.mtu = ASTEROS_ETHERNET_MTU_MAX;
    driver->interface.link_up = 1u;
    driver->interface.driver_context = driver;
    driver->interface.transmit = ne2k_transmit;
    driver->initialized = 1u;

    if (asteros_netif_register(&driver->interface) != ASTEROS_NET_OK) {
        driver->initialized = 0u;
        reg_write(driver, NE2K_REG_IMR, 0u);
        reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_STOP_ABORT);
        return ASTEROS_NE2K_NET_REGISTER_FAILED;
    }
    return ASTEROS_NE2K_OK;
}

void asteros_ne2k_stop(asteros_ne2k_driver_t *driver) {
    if (!driver || !driver->io_base || !driver->ops.write8) return;
    if (asteros_netif_is_registered(&driver->interface))
        (void)asteros_netif_set_link(&driver->interface, 0u);
    reg_write(driver, NE2K_REG_IMR, 0u);
    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_STOP_ABORT);
    driver->initialized = 0u;
}

static uint8_t previous_ring_page(uint8_t page) {
    return page == NE2K_RX_START ? (uint8_t)(NE2K_RX_STOP - 1u)
                                 : (uint8_t)(page - 1u);
}

static uint8_t ring_page_valid(uint8_t page) {
    return (uint8_t)(page >= NE2K_RX_START && page < NE2K_RX_STOP);
}

static uint8_t ring_read(asteros_ne2k_driver_t *driver, uint8_t page,
                         uint8_t *destination, uint16_t length) {
    uint16_t address = (uint16_t)(((uint16_t)page << 8) + 4u);
    uint16_t ring_end = (uint16_t)(NE2K_RX_STOP << 8);
    uint16_t first = (uint16_t)(ring_end - address);

    if (first > length) first = length;
    if (!dma_read(driver, address, destination, first)) return 0;
    if (first < length &&
        !dma_read(driver, (uint16_t)(NE2K_RX_START << 8),
                  destination + first, (uint16_t)(length - first)))
        return 0;
    return 1;
}

static void discard_receive_ring(asteros_ne2k_driver_t *driver,
                                 uint8_t current_page) {
    if (!ring_page_valid(current_page)) current_page = NE2K_RX_START;
    reg_write(driver, NE2K_REG_BOUNDARY, previous_ring_page(current_page));
}

static void recover_overflow(asteros_ne2k_driver_t *driver) {
    uint8_t current_page;

    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_STOP_ABORT);
    reg_write(driver, NE2K_REG_TCR, 0x02u);
    reg_write(driver, NE2K_REG_RBCR0, 0u);
    reg_write(driver, NE2K_REG_RBCR1, 0u);
    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_PAGE1);
    current_page = reg_read(driver, 0x07u);
    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_START_ABORT);
    discard_receive_ring(driver, current_page);
    reg_write(driver, NE2K_REG_ISR, NE2K_ISR_OVW);
    reg_write(driver, NE2K_REG_TCR, 0u);
    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_START_ABORT);
}

asteros_ne2k_poll_result_t asteros_ne2k_poll(
    asteros_ne2k_driver_t *driver) {
    uint8_t isr;
    uint8_t current_page;
    uint8_t boundary;
    uint8_t packet_page;
    uint8_t header[4];
    uint8_t next_page;
    uint8_t expected_page;
    uint8_t status;
    uint16_t count;
    uint16_t frame_length;
    uint16_t pages;
    uint16_t expected;
    asteros_net_result_t receive_result;

    if (!driver || !driver->initialized) return ASTEROS_NE2K_POLL_ERROR;

    isr = reg_read(driver, NE2K_REG_ISR);
    if (isr & NE2K_ISR_OVW) {
        recover_overflow(driver);
        return ASTEROS_NE2K_POLL_DROPPED;
    }

    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_PAGE1);
    current_page = reg_read(driver, 0x07u);
    reg_write(driver, NE2K_REG_COMMAND, NE2K_COMMAND_START_ABORT);
    if (!ring_page_valid(current_page)) {
        discard_receive_ring(driver, NE2K_RX_START);
        return ASTEROS_NE2K_POLL_ERROR;
    }

    boundary = reg_read(driver, NE2K_REG_BOUNDARY);
    if (boundary < (uint8_t)(NE2K_RX_START - 1u) ||
        boundary >= NE2K_RX_STOP) {
        discard_receive_ring(driver, current_page);
        return ASTEROS_NE2K_POLL_ERROR;
    }
    packet_page = (uint8_t)(boundary + 1u);
    if (packet_page >= NE2K_RX_STOP) packet_page = NE2K_RX_START;
    if (packet_page == current_page) return ASTEROS_NE2K_POLL_NONE;

    if (!dma_read(driver, (uint16_t)(packet_page << 8), header, sizeof header))
        return ASTEROS_NE2K_POLL_ERROR;

    status = header[0];
    next_page = header[1];
    count = (uint16_t)((uint16_t)header[2] |
                       ((uint16_t)header[3] << 8));
    if (!ring_page_valid(next_page) ||
        count < NE2K_RX_COUNT_MIN || count > NE2K_RX_COUNT_MAX) {
        discard_receive_ring(driver, current_page);
        return ASTEROS_NE2K_POLL_DROPPED;
    }

    pages = (uint16_t)((count + 4u + 255u) / 256u);
    expected = (uint16_t)packet_page + pages;
    if (expected >= NE2K_RX_STOP)
        expected = (uint16_t)(NE2K_RX_START + expected - NE2K_RX_STOP);
    expected_page = (uint8_t)expected;
    if (next_page != expected_page) {
        discard_receive_ring(driver, current_page);
        return ASTEROS_NE2K_POLL_DROPPED;
    }

    frame_length = (uint16_t)(count - 4u); /* Exclude the Ethernet FCS. */
    reg_write(driver, NE2K_REG_BOUNDARY, previous_ring_page(next_page));
    if ((status & 0x0Fu) != NE2K_RX_STATUS_GOOD)
        return ASTEROS_NE2K_POLL_DROPPED;

    if (!ring_read(driver, packet_page, driver->receive_buffer, frame_length))
        return ASTEROS_NE2K_POLL_ERROR;

    receive_result = asteros_netif_receive_frame(
        &driver->interface, driver->receive_buffer, frame_length);
    return receive_result == ASTEROS_NET_OK
        ? ASTEROS_NE2K_POLL_FRAME : ASTEROS_NE2K_POLL_DROPPED;
}

/* Real hardware bindings. The core above remains host-testable through the
   injected byte/word port operations and PCI-enable callback. */
static uint8_t hardware_read8(void *context, uint16_t port) {
    uint8_t value;
    (void)context;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void hardware_write8(void *context, uint16_t port, uint8_t value) {
    (void)context;
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static uint16_t hardware_read16(void *context, uint16_t port) {
    uint16_t value;
    (void)context;
    __asm__ volatile ("inw %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static void hardware_write16(void *context, uint16_t port, uint16_t value) {
    (void)context;
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static uint8_t hardware_enable_io(void *context,
                                  const asteros_pci_device_t *device) {
    uint16_t command;
    (void)context;
    return (uint8_t)(asteros_pci_enable_device(
        device, ASTEROS_PCI_COMMAND_IO_SPACE, &command) == ASTEROS_PCI_OK);
}

static const asteros_ne2k_ops_t hardware_ops = {
    hardware_read8,
    hardware_write8,
    hardware_read16,
    hardware_write16,
    hardware_enable_io
};

static asteros_ne2k_driver_t default_driver;

void asteros_ne2k_init_default(void) {
    uint8_t index;

    for (index = 0; index < asteros_pci_network_controller_count(); index++) {
        const asteros_pci_device_t *device =
            asteros_pci_network_controller_at(index);
        asteros_pci_resources_t resources;

        if (!device ||
            device->vendor_id != ASTEROS_NE2K_VENDOR_REALTEK ||
            device->device_id != ASTEROS_NE2K_DEVICE_RTL8029)
            continue;
        if (asteros_pci_read_device_resources(device, &resources) !=
            ASTEROS_PCI_OK)
            continue;
        if (asteros_ne2k_start(&default_driver, device, &resources,
                               &hardware_ops, 0) == ASTEROS_NE2K_OK)
            return;
    }
}

void asteros_ne2k_poll_default(void) {
    (void)asteros_ne2k_poll(&default_driver);
}

#include "pci.h"

/* Configuration mechanism #1 is safe here because the current kernel keeps
   maskable interrupts disabled and performs discovery synchronously. */
static void pci_out32(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static uint32_t pci_in32(uint16_t port) {
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static uint32_t pci_read_config32(void *context, uint8_t bus,
                                  uint8_t device, uint8_t function,
                                  uint8_t offset) {
    uint32_t address;
    (void)context;

    address = 0x80000000u |
              ((uint32_t)bus << 16) |
              ((uint32_t)(device & 0x1Fu) << 11) |
              ((uint32_t)(function & 0x07u) << 8) |
              ((uint32_t)offset & 0xFCu);
    pci_out32(0xCF8u, address);
    return pci_in32(0xCFCu);
}

asteros_pci_result_t asteros_pci_scan(
    asteros_pci_config_read32_fn read_config, void *read_context,
    asteros_pci_device_visitor_fn visit, void *visit_context,
    asteros_pci_scan_stats_t *stats) {
    uint8_t pending_buses[256];
    uint8_t queued_buses[256];
    uint16_t queue_head = 0;
    uint16_t queue_tail = 0;
    uint16_t index;

    if (stats) {
        stats->buses_scanned = 0;
        stats->devices_found = 0;
    }
    if (!read_config || !visit || !stats)
        return ASTEROS_PCI_INVALID_ARGUMENT;

    for (index = 0; index < 256u; index++) queued_buses[index] = 0;
    pending_buses[queue_tail++] = 0;
    queued_buses[0] = 1;

    while (queue_head < queue_tail) {
        uint8_t bus = pending_buses[queue_head++];
        uint8_t device;

        stats->buses_scanned++;

        for (device = 0; device < 32u; device++) {
            uint32_t identity0 = read_config(
                read_context, bus, device, 0, 0x00u);
            uint32_t header0;
            uint8_t multifunction;
            uint8_t function_count;
            uint8_t function;

            if ((identity0 & 0xFFFFu) == 0xFFFFu) continue;
            header0 = read_config(read_context, bus, device, 0, 0x0Cu);
            multifunction = (uint8_t)((header0 >> 23) & 1u);
            function_count = multifunction ? 8u : 1u;

            for (function = 0; function < function_count; function++) {
                uint32_t identity = function == 0u
                    ? identity0
                    : read_config(read_context, bus, device, function, 0x00u);
                uint32_t class_info;
                uint32_t header_info;
                uint32_t interrupt_info;
                asteros_pci_device_t found;

                if ((identity & 0xFFFFu) == 0xFFFFu) continue;

                class_info = read_config(
                    read_context, bus, device, function, 0x08u);
                header_info = function == 0u
                    ? header0
                    : read_config(read_context, bus, device, function, 0x0Cu);
                interrupt_info = read_config(
                    read_context, bus, device, function, 0x3Cu);

                found.bus = bus;
                found.device = device;
                found.function = function;
                found.vendor_id = (uint16_t)(identity & 0xFFFFu);
                found.device_id = (uint16_t)(identity >> 16);
                found.revision = (uint8_t)(class_info & 0xFFu);
                found.programming_interface = (uint8_t)(class_info >> 8);
                found.subclass = (uint8_t)(class_info >> 16);
                found.base_class = (uint8_t)(class_info >> 24);
                found.header_type = (uint8_t)((header_info >> 16) & 0x7Fu);
                found.multifunction = (uint8_t)((header_info >> 23) & 1u);
                found.interrupt_line = (uint8_t)(interrupt_info & 0xFFu);
                found.interrupt_pin = (uint8_t)((interrupt_info >> 8) & 0xFFu);

                stats->devices_found++;
                visit(&found, visit_context);

                /* PCI-to-PCI bridges use header type 1 and class 06:04.
                   Queue each configured secondary bus once; the visited set
                   also prevents loops in malformed or cyclic bridge data. */
                if (found.header_type == 1u &&
                    found.base_class == 0x06u &&
                    found.subclass == 0x04u) {
                    uint32_t buses = read_config(
                        read_context, bus, device, function, 0x18u);
                    uint8_t secondary = (uint8_t)(buses >> 8);

                    if (buses != 0xFFFFFFFFu && secondary != 0u &&
                        !queued_buses[secondary] && queue_tail < 256u) {
                        pending_buses[queue_tail++] = secondary;
                        queued_buses[secondary] = 1;
                    }
                }
            }
        }
    }

    return ASTEROS_PCI_OK;
}

static asteros_pci_device_t network_controllers[
    ASTEROS_PCI_MAX_NETWORK_CONTROLLERS];
static uint8_t network_controller_count;

static void collect_network_controller(const asteros_pci_device_t *device,
                                       void *context) {
    (void)context;
    if (device->base_class != 0x02u ||
        network_controller_count >= ASTEROS_PCI_MAX_NETWORK_CONTROLLERS)
        return;
    network_controllers[network_controller_count++] = *device;
}

void asteros_pci_init(void) {
    asteros_pci_scan_stats_t stats;

    network_controller_count = 0;
    (void)asteros_pci_scan(pci_read_config32, 0,
                           collect_network_controller, 0, &stats);
}

uint8_t asteros_pci_network_controller_count(void) {
    return network_controller_count;
}

const asteros_pci_device_t *asteros_pci_network_controller_at(uint8_t index) {
    if (index >= network_controller_count) return 0;
    return &network_controllers[index];
}

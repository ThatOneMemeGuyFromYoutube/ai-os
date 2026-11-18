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

static uint32_t pci_config_address(uint8_t bus, uint8_t device,
                                   uint8_t function, uint8_t offset) {
    return 0x80000000u |
           ((uint32_t)bus << 16) |
           ((uint32_t)(device & 0x1Fu) << 11) |
           ((uint32_t)(function & 0x07u) << 8) |
           ((uint32_t)offset & 0xFCu);
}

static uint32_t pci_read_config32(void *context, uint8_t bus,
                                  uint8_t device, uint8_t function,
                                  uint8_t offset) {
    (void)context;
    pci_out32(0xCF8u, pci_config_address(bus, device, function, offset));
    return pci_in32(0xCFCu);
}

/* Writing only the command word avoids writing back the adjacent PCI status
   register, whose asserted bits are cleared by writing ones. */
static uint8_t pci_write_config16(void *context, uint8_t bus,
                                 uint8_t device, uint8_t function,
                                 uint8_t offset, uint16_t value) {
    uint16_t data_port = (uint16_t)(0xCFCu + (offset & 0x03u));
    (void)context;
    pci_out32(0xCF8u, pci_config_address(bus, device, function, offset));
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(data_port));
    return 1;
}

static uint8_t pci_device_address_valid(const asteros_pci_device_t *device) {
    return (uint8_t)(device && device->device < 32u &&
                     device->function < 8u);
}

asteros_pci_result_t asteros_pci_read_resources(
    asteros_pci_config_read32_fn read_config, void *read_context,
    const asteros_pci_device_t *device, asteros_pci_resources_t *resources) {
    uint8_t slots;
    uint8_t index;

    if (resources) {
        resources->slot_count = 0;
        resources->count = 0;
    }
    if (!read_config || !pci_device_address_valid(device) || !resources)
        return ASTEROS_PCI_INVALID_ARGUMENT;

    switch (device->header_type) {
        case 0u: slots = 6u; break; /* Endpoint / ordinary device. */
        case 1u: slots = 2u; break; /* PCI-to-PCI bridge. */
        case 2u: slots = 1u; break; /* CardBus bridge. */
        default: return ASTEROS_PCI_UNSUPPORTED_HEADER_TYPE;
    }
    resources->slot_count = slots;

    for (index = 0; index < slots; index++) {
        uint8_t offset = (uint8_t)(0x10u + (uint8_t)(index * 4u));
        uint32_t low = read_config(read_context, device->bus, device->device,
                                   device->function, offset);
        asteros_pci_bar_t *bar;
        uint64_t base;
        uint8_t type;
        uint8_t width = 32u;
        uint8_t prefetchable = 0u;

        if (low == 0xFFFFFFFFu) return ASTEROS_PCI_CONFIG_READ_FAILED;
        if (low == 0u) continue; /* Unimplemented or unassigned BAR. */

        if (low & 1u) {
            type = ASTEROS_PCI_BAR_IO;
            base = (uint64_t)(low & 0xFFFFFFFCu);
        } else {
            uint8_t memory_type = (uint8_t)((low >> 1) & 0x03u);
            prefetchable = (uint8_t)((low >> 3) & 1u);

            if (memory_type == 0u) {
                type = ASTEROS_PCI_BAR_MEMORY;
                base = (uint64_t)(low & 0xFFFFFFF0u);
            } else if (memory_type == 2u) {
                uint32_t high;
                if ((uint8_t)(index + 1u) >= slots)
                    return ASTEROS_PCI_UNSUPPORTED_BAR_TYPE;
                high = read_config(read_context, device->bus, device->device,
                                   device->function, (uint8_t)(offset + 4u));
                if (high == 0xFFFFFFFFu)
                    return ASTEROS_PCI_CONFIG_READ_FAILED;
                type = ASTEROS_PCI_BAR_MEMORY;
                width = 64u;
                base = ((uint64_t)high << 32) |
                       (uint64_t)(low & 0xFFFFFFF0u);
                index++; /* The high dword is not an independent BAR. */
            } else {
                /* Below-1-MiB and reserved memory BAR encodings are not
                   usable by the current flat 32-bit driver model. */
                return ASTEROS_PCI_UNSUPPORTED_BAR_TYPE;
            }
        }

        if (base == 0u) continue;

        bar = &resources->bars[resources->count++];
        bar->index = (uint8_t)(width == 64u ? index - 1u : index);
        bar->type = type;
        bar->width = width;
        bar->prefetchable = prefetchable;
        bar->base_address = base;
    }

    return ASTEROS_PCI_OK;
}

asteros_pci_result_t asteros_pci_read_device_resources(
    const asteros_pci_device_t *device, asteros_pci_resources_t *resources) {
    return asteros_pci_read_resources(pci_read_config32, 0, device, resources);
}

asteros_pci_result_t asteros_pci_enable_command(
    asteros_pci_config_read32_fn read_config,
    asteros_pci_config_write16_fn write_config, void *config_context,
    const asteros_pci_device_t *device, uint16_t required_bits,
    uint16_t *enabled_command) {
    uint32_t config_value;
    uint16_t current;
    uint16_t desired;
    uint16_t verified;

    if (enabled_command) *enabled_command = 0u;
    if (!read_config || !write_config ||
        !pci_device_address_valid(device) || !enabled_command ||
        required_bits == 0u ||
        (required_bits & (uint16_t)~ASTEROS_PCI_COMMAND_RESOURCE_MASK) != 0u)
        return ASTEROS_PCI_INVALID_ARGUMENT;

    config_value = read_config(config_context, device->bus, device->device,
                               device->function, 0x04u);
    if (config_value == 0xFFFFFFFFu)
        return ASTEROS_PCI_CONFIG_READ_FAILED;

    current = (uint16_t)(config_value & 0xFFFFu);
    desired = (uint16_t)(current | required_bits);
    if (desired != current &&
        !write_config(config_context, device->bus, device->device,
                      device->function, 0x04u, desired))
        return ASTEROS_PCI_CONFIG_WRITE_FAILED;

    config_value = read_config(config_context, device->bus, device->device,
                               device->function, 0x04u);
    if (config_value == 0xFFFFFFFFu)
        return ASTEROS_PCI_CONFIG_READ_FAILED;
    verified = (uint16_t)(config_value & 0xFFFFu);
    if ((verified & required_bits) != required_bits)
        return ASTEROS_PCI_CONFIG_WRITE_FAILED;

    *enabled_command = verified;
    return ASTEROS_PCI_OK;
}

asteros_pci_result_t asteros_pci_enable_device(
    const asteros_pci_device_t *device, uint16_t required_bits,
    uint16_t *enabled_command) {
    return asteros_pci_enable_command(pci_read_config32, pci_write_config16, 0,
                                      device, required_bits, enabled_command);
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

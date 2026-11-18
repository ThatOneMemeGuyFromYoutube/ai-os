#ifndef ASTEROS_PCI_H
#define ASTEROS_PCI_H

#include <stdint.h>

#define ASTEROS_PCI_MAX_NETWORK_CONTROLLERS 4u
#define ASTEROS_PCI_BAR_MAX 6u

#define ASTEROS_PCI_COMMAND_IO_SPACE 0x0001u
#define ASTEROS_PCI_COMMAND_MEMORY_SPACE 0x0002u
#define ASTEROS_PCI_COMMAND_BUS_MASTER 0x0004u
#define ASTEROS_PCI_COMMAND_RESOURCE_MASK \
    (ASTEROS_PCI_COMMAND_IO_SPACE | ASTEROS_PCI_COMMAND_MEMORY_SPACE | \
     ASTEROS_PCI_COMMAND_BUS_MASTER)

typedef struct {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t revision;
    uint8_t programming_interface;
    uint8_t subclass;
    uint8_t base_class;
    uint8_t header_type;
    uint8_t multifunction;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
} asteros_pci_device_t;

typedef uint32_t (*asteros_pci_config_read32_fn)(
    void *context, uint8_t bus, uint8_t device, uint8_t function,
    uint8_t offset);
typedef uint8_t (*asteros_pci_config_write16_fn)(
    void *context, uint8_t bus, uint8_t device, uint8_t function,
    uint8_t offset, uint16_t value);
typedef void (*asteros_pci_device_visitor_fn)(
    const asteros_pci_device_t *device, void *context);

typedef struct {
    uint16_t buses_scanned;
    uint32_t devices_found;
} asteros_pci_scan_stats_t;

typedef enum {
    ASTEROS_PCI_BAR_IO = 1,
    ASTEROS_PCI_BAR_MEMORY = 2
} asteros_pci_bar_type_t;

typedef struct {
    uint8_t index;
    uint8_t type;
    uint8_t width;
    uint8_t prefetchable;
    uint64_t base_address;
} asteros_pci_bar_t;

typedef struct {
    uint8_t slot_count;
    uint8_t count;
    asteros_pci_bar_t bars[ASTEROS_PCI_BAR_MAX];
} asteros_pci_resources_t;

typedef enum {
    ASTEROS_PCI_OK = 0,
    ASTEROS_PCI_INVALID_ARGUMENT,
    ASTEROS_PCI_UNSUPPORTED_HEADER_TYPE,
    ASTEROS_PCI_UNSUPPORTED_BAR_TYPE,
    ASTEROS_PCI_CONFIG_READ_FAILED,
    ASTEROS_PCI_CONFIG_WRITE_FAILED
} asteros_pci_result_t;

/* Enumerate conventional PCI buses reachable from bus 0. The config reader
   and visitor are injected so bus traversal can be tested without port I/O.
   The visitor receives a temporary record and must copy it to retain it. */
asteros_pci_result_t asteros_pci_scan(
    asteros_pci_config_read32_fn read_config, void *read_context,
    asteros_pci_device_visitor_fn visit, void *visit_context,
    asteros_pci_scan_stats_t *stats);

/* Decode currently assigned BAR base addresses without probing or changing
   hardware. A 64-bit memory BAR consumes the following BAR slot. Only
   conventional endpoint, PCI bridge, and CardBus header layouts are read. */
asteros_pci_result_t asteros_pci_read_resources(
    asteros_pci_config_read32_fn read_config, void *read_context,
    const asteros_pci_device_t *device, asteros_pci_resources_t *resources);

/* Enable requested PCI command-register decode/master bits. This only sets
   supported bits, preserves every existing command bit, writes the 16-bit
   command half (never the adjacent write-one-to-clear status half), and
   verifies the result. Drivers must first validate their assigned BARs. */
asteros_pci_result_t asteros_pci_enable_command(
    asteros_pci_config_read32_fn read_config,
    asteros_pci_config_write16_fn write_config, void *config_context,
    const asteros_pci_device_t *device, uint16_t required_bits,
    uint16_t *enabled_command);

/* Kernel startup uses PCI configuration mechanism #1 (0xCF8/0xCFC). This
   read-only pass records up to four class-02 network controllers for drivers. */
void asteros_pci_init(void);
uint8_t asteros_pci_network_controller_count(void);
const asteros_pci_device_t *asteros_pci_network_controller_at(uint8_t index);

#endif

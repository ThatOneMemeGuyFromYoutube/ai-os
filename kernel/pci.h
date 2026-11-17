#ifndef ASTEROS_PCI_H
#define ASTEROS_PCI_H

#include <stdint.h>

#define ASTEROS_PCI_MAX_NETWORK_CONTROLLERS 4u

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
typedef void (*asteros_pci_device_visitor_fn)(
    const asteros_pci_device_t *device, void *context);

typedef struct {
    uint16_t buses_scanned;
    uint32_t devices_found;
} asteros_pci_scan_stats_t;

typedef enum {
    ASTEROS_PCI_OK = 0,
    ASTEROS_PCI_INVALID_ARGUMENT
} asteros_pci_result_t;

/* Enumerate conventional PCI buses reachable from bus 0. The config reader
   and visitor are injected so bus traversal can be tested without port I/O.
   The visitor receives a temporary record and must copy it to retain it. */
asteros_pci_result_t asteros_pci_scan(
    asteros_pci_config_read32_fn read_config, void *read_context,
    asteros_pci_device_visitor_fn visit, void *visit_context,
    asteros_pci_scan_stats_t *stats);

/* Kernel startup uses PCI configuration mechanism #1 (0xCF8/0xCFC). This
   read-only pass records up to four class-02 network controllers for drivers. */
void asteros_pci_init(void);
uint8_t asteros_pci_network_controller_count(void);
const asteros_pci_device_t *asteros_pci_network_controller_at(uint8_t index);

#endif

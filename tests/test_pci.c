#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pci.h"

typedef struct {
    uint8_t bus, device, function;
    uint16_t vendor, device_id;
    uint8_t revision, prog_if, subclass, base_class;
    uint8_t header_type, interrupt_line, interrupt_pin, secondary_bus;
} pci_fixture_t;

static pci_fixture_t fixtures[32];
static size_t fixture_count;
static asteros_pci_device_t visited[32];
static size_t visited_count;

static void add_device(uint8_t bus, uint8_t device, uint8_t function,
                       uint16_t vendor, uint16_t device_id,
                       uint8_t base_class, uint8_t subclass,
                       uint8_t prog_if, uint8_t header_type,
                       uint8_t revision, uint8_t interrupt_line,
                       uint8_t interrupt_pin, uint8_t secondary_bus) {
    pci_fixture_t *fixture;
    assert(fixture_count < sizeof fixtures / sizeof fixtures[0]);
    fixture = &fixtures[fixture_count++];
    fixture->bus = bus;
    fixture->device = device;
    fixture->function = function;
    fixture->vendor = vendor;
    fixture->device_id = device_id;
    fixture->base_class = base_class;
    fixture->subclass = subclass;
    fixture->prog_if = prog_if;
    fixture->header_type = header_type;
    fixture->revision = revision;
    fixture->interrupt_line = interrupt_line;
    fixture->interrupt_pin = interrupt_pin;
    fixture->secondary_bus = secondary_bus;
}

static uint32_t fake_read_config(void *context, uint8_t bus,
                                 uint8_t device, uint8_t function,
                                 uint8_t offset) {
    size_t index;
    (void)context;

    for (index = 0; index < fixture_count; index++) {
        const pci_fixture_t *fixture = &fixtures[index];
        if (fixture->bus != bus || fixture->device != device ||
            fixture->function != function)
            continue;

        switch (offset) {
            case 0x00:
                return (uint32_t)fixture->vendor |
                       ((uint32_t)fixture->device_id << 16);
            case 0x08:
                return (uint32_t)fixture->revision |
                       ((uint32_t)fixture->prog_if << 8) |
                       ((uint32_t)fixture->subclass << 16) |
                       ((uint32_t)fixture->base_class << 24);
            case 0x0C:
                return (uint32_t)fixture->header_type << 16;
            case 0x18:
                return (uint32_t)fixture->secondary_bus << 8;
            case 0x3C:
                return (uint32_t)fixture->interrupt_line |
                       ((uint32_t)fixture->interrupt_pin << 8);
            default:
                return 0;
        }
    }
    return 0xFFFFFFFFu;
}

static void capture_device(const asteros_pci_device_t *device, void *context) {
    (void)context;
    assert(visited_count < sizeof visited / sizeof visited[0]);
    visited[visited_count++] = *device;
}

static void reset_fixtures(void) {
    fixture_count = 0;
    visited_count = 0;
    memset(fixtures, 0, sizeof fixtures);
    memset(visited, 0, sizeof visited);
}

static void test_bridge_and_multifunction_enumeration(void) {
    asteros_pci_scan_stats_t stats;

    reset_fixtures();
    add_device(0, 0, 0, 0x8086, 0x1237, 0x06, 0x00, 0, 0x00, 2, 0, 0, 0);
    add_device(0, 2, 0, 0x10EC, 0x8029, 0x02, 0x00, 0, 0x00, 5, 11, 1, 0);
    add_device(0, 3, 0, 0x1AF4, 0x1000, 0x06, 0x04, 0, 0x01, 1, 10, 1, 2);
    add_device(0, 4, 0, 0x1234, 0x1111, 0x03, 0x00, 0, 0x80, 1, 0, 0, 0);
    add_device(0, 4, 1, 0x1234, 0x2222, 0x04, 0x01, 0, 0x00, 1, 0, 0, 0);
    add_device(0, 4, 2, 0x1234, 0x3333, 0x02, 0x00, 0, 0x00, 1, 9, 1, 0);
    add_device(0, 5, 0, 0x1AF4, 0x1001, 0x06, 0x04, 0, 0x01, 1, 10, 1, 2);
    add_device(0, 6, 0, 0x1234, 0x4444, 0x06, 0x04, 0, 0x00, 1, 0, 0, 9);
    add_device(0, 7, 0, 0x1234, 0x5555, 0x06, 0x00, 0, 0x00, 1, 0, 0, 0);
    /* Function 1 must be ignored when function 0 is not multifunction. */
    add_device(0, 7, 1, 0x10EC, 0x8029, 0x02, 0x00, 0, 0x00, 1, 5, 1, 0);
    add_device(2, 0, 0, 0x1AF4, 0x2000, 0x02, 0x00, 0, 0x00, 1, 11, 1, 0);
    add_device(2, 1, 0, 0x1AF4, 0x1000, 0x06, 0x04, 0, 0x01, 1, 10, 1, 5);
    add_device(5, 0, 0, 0x1AF4, 0x3000, 0x02, 0x00, 0, 0x00, 1, 12, 1, 0);
    /* A bridge cycle back to bus 2 must not enqueue the bus twice. */
    add_device(5, 1, 0, 0x1AF4, 0x1000, 0x06, 0x04, 0, 0x01, 1, 10, 1, 2);

    assert(asteros_pci_scan(fake_read_config, 0, capture_device, 0, &stats) ==
           ASTEROS_PCI_OK);
    assert(stats.buses_scanned == 3);
    assert(stats.devices_found == 13);
    assert(visited_count == 13);

    assert(visited[0].bus == 0 && visited[0].device == 0);
    assert(visited[1].vendor_id == 0x10EC);
    assert(visited[1].device_id == 0x8029);
    assert(visited[1].base_class == 0x02);
    assert(visited[1].revision == 5);
    assert(visited[1].interrupt_line == 11);
    assert(visited[1].interrupt_pin == 1);

    assert(visited[3].device == 4 && visited[3].function == 0);
    assert(visited[3].multifunction == 1);
    assert(visited[4].device == 4 && visited[4].function == 1);
    assert(visited[5].device == 4 && visited[5].function == 2);
    assert(visited[6].device == 5);
    assert(visited[7].device == 6 && visited[7].header_type == 0);
    assert(visited[8].device == 7 && visited[8].function == 0);
    assert(visited[9].bus == 2 && visited[9].device == 0);
    assert(visited[10].bus == 2 && visited[10].device == 1);
    assert(visited[11].bus == 5 && visited[11].device == 0);
    assert(visited[12].bus == 5 && visited[12].device == 1);
}

static void test_bus_255_and_unconfigured_bridges(void) {
    asteros_pci_scan_stats_t stats;

    reset_fixtures();
    add_device(0, 1, 0, 0x1AF4, 0x1000, 0x06, 0x04, 0, 0x01, 1, 0, 0, 255);
    add_device(0, 2, 0, 0x1AF4, 0x1001, 0x06, 0x04, 0, 0x01, 1, 0, 0, 0);
    add_device(255, 0, 0, 0x10EC, 0x8029, 0x02, 0x00, 0, 0x00, 1, 9, 1, 0);

    assert(asteros_pci_scan(fake_read_config, 0, capture_device, 0, &stats) ==
           ASTEROS_PCI_OK);
    assert(stats.buses_scanned == 2);
    assert(stats.devices_found == 3);
    assert(visited_count == 3);
    assert(visited[2].bus == 255);
}

static void test_invalid_callbacks_are_rejected(void) {
    asteros_pci_scan_stats_t stats = {99, 99};

    assert(asteros_pci_scan(0, 0, capture_device, 0, &stats) ==
           ASTEROS_PCI_INVALID_ARGUMENT);
    assert(stats.buses_scanned == 0 && stats.devices_found == 0);
    assert(asteros_pci_scan(fake_read_config, 0, 0, 0, &stats) ==
           ASTEROS_PCI_INVALID_ARGUMENT);
    assert(asteros_pci_scan(fake_read_config, 0, capture_device, 0, 0) ==
           ASTEROS_PCI_INVALID_ARGUMENT);
}

int main(void) {
    test_bridge_and_multifunction_enumeration();
    test_bus_255_and_unconfigured_bridges();
    test_invalid_callbacks_are_rejected();
    puts("PCI enumeration: tests passed");
    return 0;
}

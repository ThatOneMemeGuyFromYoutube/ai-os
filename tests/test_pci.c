#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pci.h"

typedef struct {
    uint8_t bus, device, function;
    uint16_t vendor, device_id;
    uint8_t revision, prog_if, subclass, base_class;
    uint8_t header_type, interrupt_line, interrupt_pin, secondary_bus;
    uint32_t bars[ASTEROS_PCI_BAR_MAX];
    uint16_t command, status;
    uint16_t last_write;
    uint8_t write_success, write_count, last_write_offset;
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
    fixture->write_success = 1;
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
            case 0x04:
                return (uint32_t)fixture->command |
                       ((uint32_t)fixture->status << 16);
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
                if (offset >= 0x10u && offset <= 0x24u &&
                    (offset & 3u) == 0u)
                    return fixture->bars[(offset - 0x10u) / 4u];
                return 0xFFFFFFFFu;
            /* Keep the switch exhaustive for offsets not modeled above. */
            case 0xFF:
                return 0;
        }
    }
    return 0xFFFFFFFFu;
}

static pci_fixture_t *find_fixture(uint8_t bus, uint8_t device,
                                   uint8_t function) {
    size_t index;
    for (index = 0; index < fixture_count; index++) {
        if (fixtures[index].bus == bus && fixtures[index].device == device &&
            fixtures[index].function == function)
            return &fixtures[index];
    }
    return 0;
}

static uint8_t fake_write_config16(void *context, uint8_t bus,
                                   uint8_t device, uint8_t function,
                                   uint8_t offset, uint16_t value) {
    pci_fixture_t *fixture = find_fixture(bus, device, function);
    (void)context;
    if (!fixture || offset != 0x04u) return 0;
    fixture->write_count++;
    fixture->last_write_offset = offset;
    fixture->last_write = value;
    if (!fixture->write_success) return 0;
    fixture->command = value;
    return 1;
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

static void test_endpoint_bar_decoding(void) {
    asteros_pci_device_t device;
    asteros_pci_resources_t resources;
    pci_fixture_t *fixture;

    reset_fixtures();
    add_device(0, 1, 0, 0x10EC, 0x8029, 0x02, 0x00, 0, 0x00, 1, 9, 1, 0);
    fixture = find_fixture(0, 1, 0);
    assert(fixture != 0);
    fixture->bars[0] = 0x0000C001u; /* I/O BAR at 0xC000. */
    fixture->bars[1] = 0xFEB00008u; /* Prefetchable 32-bit memory BAR. */
    fixture->bars[2] = 0x0000200Cu; /* Prefetchable 64-bit memory BAR. */
    fixture->bars[3] = 0x00000001u; /* High dword, not a separate BAR. */
    fixture->bars[4] = 0x0000D001u; /* A later I/O BAR remains visible. */

    memset(&device, 0, sizeof device);
    device.bus = 0;
    device.device = 1;
    device.function = 0;
    device.header_type = 0;
    assert(asteros_pci_read_resources(fake_read_config, 0, &device,
                                      &resources) == ASTEROS_PCI_OK);
    assert(resources.slot_count == 6);
    assert(resources.count == 4);

    assert(resources.bars[0].index == 0);
    assert(resources.bars[0].type == ASTEROS_PCI_BAR_IO);
    assert(resources.bars[0].width == 32);
    assert(resources.bars[0].prefetchable == 0);
    assert(resources.bars[0].base_address == 0xC000u);

    assert(resources.bars[1].index == 1);
    assert(resources.bars[1].type == ASTEROS_PCI_BAR_MEMORY);
    assert(resources.bars[1].width == 32);
    assert(resources.bars[1].prefetchable == 1);
    assert(resources.bars[1].base_address == 0xFEB00000u);

    assert(resources.bars[2].index == 2);
    assert(resources.bars[2].type == ASTEROS_PCI_BAR_MEMORY);
    assert(resources.bars[2].width == 64);
    assert(resources.bars[2].prefetchable == 1);
    assert(resources.bars[2].base_address == 0x0000000100002000ULL);

    assert(resources.bars[3].index == 4);
    assert(resources.bars[3].type == ASTEROS_PCI_BAR_IO);
    assert(resources.bars[3].base_address == 0xD000u);
}

static void test_bridge_bar_layout_and_unassigned_slots(void) {
    asteros_pci_device_t device;
    asteros_pci_resources_t resources;
    pci_fixture_t *fixture;

    reset_fixtures();
    add_device(0, 2, 0, 0x1234, 0x5678, 0x06, 0x04, 0, 0x01, 1, 0, 0, 0);
    fixture = find_fixture(0, 2, 0);
    assert(fixture != 0);
    fixture->bars[0] = 0x0000E001u;
    fixture->bars[1] = 0u;
    fixture->bars[2] = 0x0000F001u; /* Outside a type-1 header's BAR area. */

    memset(&device, 0, sizeof device);
    device.device = 2;
    device.header_type = 1;
    assert(asteros_pci_read_resources(fake_read_config, 0, &device,
                                      &resources) == ASTEROS_PCI_OK);
    assert(resources.slot_count == 2);
    assert(resources.count == 1);
    assert(resources.bars[0].index == 0);
    assert(resources.bars[0].base_address == 0xE000u);
}

static void test_invalid_and_unsupported_bar_layouts(void) {
    asteros_pci_device_t device;
    asteros_pci_resources_t resources;
    pci_fixture_t *fixture;

    reset_fixtures();
    add_device(0, 3, 0, 0x1234, 0x1111, 0x02, 0x00, 0, 0x00, 1, 0, 0, 0);
    fixture = find_fixture(0, 3, 0);
    assert(fixture != 0);
    memset(&device, 0, sizeof device);
    device.device = 3;
    device.header_type = 0;

    fixture->bars[0] = 0x00000004u; /* 64-bit BAR cannot occupy final slot. */
    assert(asteros_pci_read_resources(fake_read_config, 0, &device,
                                      &resources) == ASTEROS_PCI_OK);
    /* BAR0 is a valid 64-bit BAR; its upper slot is BAR1. */
    fixture->bars[5] = 0x00000004u;
    assert(asteros_pci_read_resources(fake_read_config, 0, &device,
                                      &resources) == ASTEROS_PCI_UNSUPPORTED_BAR_TYPE);

    fixture->bars[5] = 0u;
    fixture->bars[0] = 0x00000002u; /* Below-1-MiB memory BAR. */
    assert(asteros_pci_read_resources(fake_read_config, 0, &device,
                                      &resources) == ASTEROS_PCI_UNSUPPORTED_BAR_TYPE);

    fixture->bars[0] = 0x00000006u; /* Reserved memory BAR encoding. */
    assert(asteros_pci_read_resources(fake_read_config, 0, &device,
                                      &resources) == ASTEROS_PCI_UNSUPPORTED_BAR_TYPE);

    fixture->bars[0] = 0xFFFFFFFFu;
    assert(asteros_pci_read_resources(fake_read_config, 0, &device,
                                      &resources) == ASTEROS_PCI_CONFIG_READ_FAILED);

    device.header_type = 0x7Fu;
    assert(asteros_pci_read_resources(fake_read_config, 0, &device,
                                      &resources) == ASTEROS_PCI_UNSUPPORTED_HEADER_TYPE);
    assert(asteros_pci_read_resources(0, 0, &device, &resources) ==
           ASTEROS_PCI_INVALID_ARGUMENT);
    assert(asteros_pci_read_resources(fake_read_config, 0, 0, &resources) ==
           ASTEROS_PCI_INVALID_ARGUMENT);
    assert(asteros_pci_read_resources(fake_read_config, 0, &device, 0) ==
           ASTEROS_PCI_INVALID_ARGUMENT);
}

static void test_command_enable_preserves_status_and_existing_bits(void) {
    asteros_pci_device_t device;
    pci_fixture_t *fixture;
    uint16_t enabled = 0;

    reset_fixtures();
    add_device(0, 4, 0, 0x10EC, 0x8029, 0x02, 0x00, 0, 0x00, 1, 9, 1, 0);
    fixture = find_fixture(0, 4, 0);
    assert(fixture != 0);
    fixture->command = 0x0400u;
    fixture->status = 0xF900u;

    memset(&device, 0, sizeof device);
    device.device = 4;
    assert(asteros_pci_enable_command(
        fake_read_config, fake_write_config16, 0, &device,
        ASTEROS_PCI_COMMAND_IO_SPACE | ASTEROS_PCI_COMMAND_BUS_MASTER,
        &enabled) == ASTEROS_PCI_OK);
    assert(enabled == 0x0405u);
    assert(fixture->command == 0x0405u);
    assert(fixture->status == 0xF900u);
    assert(fixture->write_count == 1);
    assert(fixture->last_write_offset == 0x04u);
    assert(fixture->last_write == 0x0405u);

    /* Repeating an enable is idempotent and performs no redundant write. */
    assert(asteros_pci_enable_command(
        fake_read_config, fake_write_config16, 0, &device,
        ASTEROS_PCI_COMMAND_IO_SPACE | ASTEROS_PCI_COMMAND_BUS_MASTER,
        &enabled) == ASTEROS_PCI_OK);
    assert(fixture->write_count == 1);
    assert(enabled == 0x0405u);

    assert(asteros_pci_enable_command(
        fake_read_config, fake_write_config16, 0, &device, 0x8000u,
        &enabled) == ASTEROS_PCI_INVALID_ARGUMENT);
    assert(asteros_pci_enable_command(
        fake_read_config, fake_write_config16, 0, &device,
        ASTEROS_PCI_COMMAND_MEMORY_SPACE, 0) == ASTEROS_PCI_INVALID_ARGUMENT);
    assert(asteros_pci_enable_command(
        fake_read_config, 0, 0, &device,
        ASTEROS_PCI_COMMAND_MEMORY_SPACE, &enabled) == ASTEROS_PCI_INVALID_ARGUMENT);
}

static void test_command_write_failure_is_reported(void) {
    asteros_pci_device_t device;
    pci_fixture_t *fixture;
    uint16_t enabled = 0xFFFFu;

    reset_fixtures();
    add_device(0, 5, 0, 0x1234, 0x2222, 0x02, 0x00, 0, 0x00, 1, 0, 0, 0);
    fixture = find_fixture(0, 5, 0);
    assert(fixture != 0);
    fixture->write_success = 0;
    memset(&device, 0, sizeof device);
    device.device = 5;

    assert(asteros_pci_enable_command(
        fake_read_config, fake_write_config16, 0, &device,
        ASTEROS_PCI_COMMAND_MEMORY_SPACE, &enabled) ==
        ASTEROS_PCI_CONFIG_WRITE_FAILED);
    assert(enabled == 0u);
    assert(fixture->command == 0u);
    assert(fixture->write_count == 1u);
}

int main(void) {
    test_bridge_and_multifunction_enumeration();
    test_bus_255_and_unconfigured_bridges();
    test_invalid_callbacks_are_rejected();
    test_endpoint_bar_decoding();
    test_bridge_bar_layout_and_unassigned_slots();
    test_invalid_and_unsupported_bar_layouts();
    test_command_enable_preserves_status_and_existing_bits();
    test_command_write_failure_is_reported();
    puts("PCI discovery/resources: tests passed");
    return 0;
}

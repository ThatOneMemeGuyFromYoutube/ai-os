#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "ata_pio.h"

#define ATA_BASE 0x1F0u
#define ATA_CONTROL 0x3F6u

typedef struct {
    uint8_t status;
    uint8_t lba_mid;
    uint8_t lba_high;
    uint8_t drive_head;
    uint8_t sector_count;
    uint8_t lba_low;
    uint8_t lba_reg_mid;
    uint8_t lba_reg_high;
    uint8_t command;
    uint8_t no_device;
    uint8_t stuck_busy;
    uint8_t device_error;
    uint16_t identify[256];
    uint16_t read_words[256];
    uint16_t written_words[256];
    uint16_t data_index;
    uint16_t write_index;
} fake_ata_t;

static uint8_t fake_in8(void *context, uint16_t port) {
    fake_ata_t *fake = (fake_ata_t *)context;
    if (port == ATA_BASE + 7u || port == ATA_CONTROL) return fake->status;
    if (port == ATA_BASE + 4u) return fake->lba_mid;
    if (port == ATA_BASE + 5u) return fake->lba_high;
    return 0;
}

static uint16_t fake_in16(void *context, uint16_t port) {
    fake_ata_t *fake = (fake_ata_t *)context;
    uint16_t value = 0;
    assert(port == ATA_BASE);
    if (fake->command == 0xECu) {
        value = fake->identify[fake->data_index++];
    } else if (fake->command == 0x20u) {
        value = fake->read_words[fake->data_index++];
    }
    if (fake->data_index == 256u) fake->status = 0x50u;
    return value;
}

static void fake_out8(void *context, uint16_t port, uint8_t value) {
    fake_ata_t *fake = (fake_ata_t *)context;
    if (port == ATA_CONTROL) return;
    if (port == ATA_BASE + 6u) {
        fake->drive_head = value;
        return;
    }
    if (port == ATA_BASE + 2u) { fake->sector_count = value; return; }
    if (port == ATA_BASE + 3u) { fake->lba_low = value; return; }
    if (port == ATA_BASE + 4u) { fake->lba_reg_mid = value; return; }
    if (port == ATA_BASE + 5u) { fake->lba_reg_high = value; return; }
    if (port == ATA_BASE + 7u) {
        fake->command = value;
        fake->data_index = 0;
        if (value == 0x30u) fake->write_index = 0;
        if (fake->no_device) fake->status = 0;
        else if (fake->stuck_busy) fake->status = 0x80u;
        else if (fake->device_error) fake->status = 0x01u;
        else if (value == 0xECu || value == 0x20u || value == 0x30u)
            fake->status = 0x08u;
        else fake->status = 0x50u;
    }
}

static void fake_out16(void *context, uint16_t port, uint16_t value) {
    fake_ata_t *fake = (fake_ata_t *)context;
    assert(port == ATA_BASE);
    assert(fake->write_index < 256u);
    fake->written_words[fake->write_index++] = value;
    if (fake->write_index == 256u) fake->status = 0x50u;
}

static const asteros_ata_io_t fake_io_template = {
    0, fake_in8, fake_in16, fake_out8, fake_out16
};

static void fake_init(fake_ata_t *fake, asteros_ata_io_t *io) {
    uint16_t index;
    memset(fake, 0, sizeof *fake);
    fake->status = 0x50u;
    fake->identify[49] = 0x0200u;
    fake->identify[60] = 0x0000u;
    fake->identify[61] = 0x0200u;
    for (index = 0; index < 256u; index++)
        fake->read_words[index] = (uint16_t)(0xA000u + index);
    *io = fake_io_template;
    io->context = fake;
}

static void probe_ok(fake_ata_t *fake, asteros_ata_io_t *io,
                     asteros_ata_device_t *device) {
    fake_init(fake, io);
    assert(asteros_ata_pio_probe(device, io, ATA_BASE, ATA_CONTROL, 0) ==
           ASTEROS_ATA_OK);
    assert(device->present == 1u);
    assert(device->sectors == 0x02000000u);
}

static void test_probe_and_rejections(void) {
    fake_ata_t fake;
    asteros_ata_io_t io;
    asteros_ata_device_t device;

    probe_ok(&fake, &io, &device);
    assert(fake.command == 0xECu);
    assert(fake.drive_head == 0xA0u);

    fake_init(&fake, &io);
    fake.no_device = 1;
    assert(asteros_ata_pio_probe(&device, &io, ATA_BASE, ATA_CONTROL, 0) ==
           ASTEROS_ATA_NO_DEVICE);
    assert(!device.present);

    fake_init(&fake, &io);
    fake.lba_mid = 0x14u;
    fake.lba_high = 0xEBu;
    assert(asteros_ata_pio_probe(&device, &io, ATA_BASE, ATA_CONTROL, 0) ==
           ASTEROS_ATA_UNSUPPORTED);

    fake_init(&fake, &io);
    fake.identify[49] = 0;
    assert(asteros_ata_pio_probe(&device, &io, ATA_BASE, ATA_CONTROL, 0) ==
           ASTEROS_ATA_UNSUPPORTED);

    fake_init(&fake, &io);
    fake.identify[60] = 0;
    fake.identify[61] = 0;
    assert(asteros_ata_pio_probe(&device, &io, ATA_BASE, ATA_CONTROL, 0) ==
           ASTEROS_ATA_UNSUPPORTED);

    fake_init(&fake, &io);
    fake.stuck_busy = 1;
    assert(asteros_ata_pio_probe(&device, &io, ATA_BASE, ATA_CONTROL, 0) ==
           ASTEROS_ATA_TIMEOUT);

    fake_init(&fake, &io);
    fake.device_error = 1;
    assert(asteros_ata_pio_probe(&device, &io, ATA_BASE, ATA_CONTROL, 0) ==
           ASTEROS_ATA_DEVICE_ERROR);

    assert(asteros_ata_pio_probe(0, &io, ATA_BASE, ATA_CONTROL, 0) ==
           ASTEROS_ATA_INVALID_ARGUMENT);
    assert(asteros_ata_pio_probe(&device, &io, ATA_BASE, ATA_CONTROL, 2) ==
           ASTEROS_ATA_INVALID_ARGUMENT);
}

static void test_sector_read_and_bounds(void) {
    fake_ata_t fake;
    asteros_ata_io_t io;
    asteros_ata_device_t device;
    uint8_t sector[ASTEROS_ATA_SECTOR_SIZE];

    probe_ok(&fake, &io, &device);
    assert(asteros_ata_pio_read(&device, 0x01234567u, sector) == ASTEROS_ATA_OK);
    assert(fake.command == 0x20u);
    assert(fake.sector_count == 1u);
    assert(fake.lba_low == 0x67u);
    assert(fake.lba_reg_mid == 0x45u);
    assert(fake.lba_reg_high == 0x23u);
    assert(fake.drive_head == 0xE1u);
    assert(sector[0] == 0x00u && sector[1] == 0xA0u);
    assert(sector[510] == 0xFFu && sector[511] == 0xA0u);

    assert(asteros_ata_pio_read(&device, device.sectors, sector) ==
           ASTEROS_ATA_OUT_OF_RANGE);
    assert(asteros_ata_pio_read(&device, 0x10000000u, sector) ==
           ASTEROS_ATA_OUT_OF_RANGE);
    assert(asteros_ata_pio_read(&device, 0, 0) ==
           ASTEROS_ATA_INVALID_ARGUMENT);
}

static void test_sector_write_and_flush(void) {
    fake_ata_t fake;
    asteros_ata_io_t io;
    asteros_ata_device_t device;
    uint8_t sector[ASTEROS_ATA_SECTOR_SIZE];
    uint16_t index;

    probe_ok(&fake, &io, &device);
    for (index = 0; index < ASTEROS_ATA_SECTOR_SIZE; index++)
        sector[index] = (uint8_t)index;
    assert(asteros_ata_pio_write(&device, 7u, sector) == ASTEROS_ATA_OK);
    assert(fake.write_index == 256u);
    assert(fake.written_words[0] == 0x0100u);
    assert(fake.written_words[255] == 0xFFFEu);
    assert(fake.command == 0xE7u);
    assert(asteros_ata_pio_write(&device, device.sectors, sector) ==
           ASTEROS_ATA_OUT_OF_RANGE);
}

int main(void) {
    test_probe_and_rejections();
    test_sector_read_and_bounds();
    test_sector_write_and_flush();
    puts("ata-pio: tests passed");
    return 0;
}

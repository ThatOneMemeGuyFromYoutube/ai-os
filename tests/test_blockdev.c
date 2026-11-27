#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "blockdev.h"

#define TEST_DISK_SECTORS 256u

typedef struct {
    uint32_t reads;
    uint32_t writes;
    uint32_t last_read_lba;
    uint32_t last_write_lba;
    uint8_t fail_read;
    uint8_t fail_write;
    uint8_t last_write[ASTEROS_BLOCKDEV_SECTOR_SIZE];
} fake_disk_t;

static uint8_t fake_read_sector(void *context, uint32_t lba,
                                uint8_t *sector) {
    fake_disk_t *disk = (fake_disk_t *)context;
    uint16_t index;
    disk->reads++;
    disk->last_read_lba = lba;
    if (disk->fail_read || lba >= TEST_DISK_SECTORS) return 0u;
    for (index = 0; index < ASTEROS_BLOCKDEV_SECTOR_SIZE; index++)
        sector[index] = (uint8_t)(lba + index);
    return 1u;
}

static uint8_t fake_write_sector(void *context, uint32_t lba,
                                 const uint8_t *sector) {
    fake_disk_t *disk = (fake_disk_t *)context;
    disk->writes++;
    disk->last_write_lba = lba;
    if (disk->fail_write || lba >= TEST_DISK_SECTORS) return 0u;
    memcpy(disk->last_write, sector, sizeof disk->last_write);
    return 1u;
}

static asteros_volume_t valid_volume(void) {
    asteros_volume_t volume;
    volume.present = 1u;
    volume.partition_index = 2u;
    volume.start_lba = 100u;
    volume.sector_count = 40u;
    volume.data_start_lba = 101u;
    volume.data_sector_count = 39u;
    return volume;
}

static void test_relative_io_stays_inside_data_region(void) {
    fake_disk_t disk;
    asteros_volume_t volume = valid_volume();
    asteros_blockdev_t device;
    uint8_t sector[ASTEROS_BLOCKDEV_SECTOR_SIZE];
    uint16_t index;

    memset(&disk, 0, sizeof disk);
    assert(asteros_blockdev_init(&device, &volume, &disk,
        fake_read_sector, fake_write_sector) == ASTEROS_BLOCKDEV_OK);
    assert(device.present == 1u);
    assert(device.start_lba == 101u);
    assert(device.sector_count == 39u);

    assert(asteros_blockdev_read(&device, 0u, sector) == ASTEROS_BLOCKDEV_OK);
    assert(disk.last_read_lba == 101u);
    assert(sector[0] == (uint8_t)101u);
    assert(sector[511] == (uint8_t)(101u + 511u));

    for (index = 0; index < sizeof sector; index++)
        sector[index] = (uint8_t)(index ^ 0xA5u);
    assert(asteros_blockdev_write(&device, 38u, sector) ==
           ASTEROS_BLOCKDEV_OK);
    assert(disk.last_write_lba == 139u);
    assert(disk.last_write_lba != 0u);
    assert(disk.last_write_lba != volume.start_lba);
    assert(memcmp(disk.last_write, sector, sizeof sector) == 0);

    {
        uint32_t reads = disk.reads;
        uint32_t writes = disk.writes;
        assert(asteros_blockdev_read(&device, 39u, sector) ==
               ASTEROS_BLOCKDEV_OUT_OF_RANGE);
        assert(asteros_blockdev_write(&device, 39u, sector) ==
               ASTEROS_BLOCKDEV_OUT_OF_RANGE);
        assert(disk.reads == reads);
        assert(disk.writes == writes);
    }
}

static void test_read_only_and_io_failures(void) {
    fake_disk_t disk;
    asteros_volume_t volume = valid_volume();
    asteros_blockdev_t device;
    uint8_t sector[ASTEROS_BLOCKDEV_SECTOR_SIZE];

    memset(&disk, 0, sizeof disk);
    assert(asteros_blockdev_init(&device, &volume, &disk,
        fake_read_sector, 0) == ASTEROS_BLOCKDEV_OK);
    assert(asteros_blockdev_write(&device, 0u, sector) ==
           ASTEROS_BLOCKDEV_READ_ONLY);
    assert(disk.writes == 0u);

    disk.fail_read = 1u;
    assert(asteros_blockdev_read(&device, 0u, sector) ==
           ASTEROS_BLOCKDEV_IO_ERROR);

    disk.fail_read = 0u;
    assert(asteros_blockdev_init(&device, &volume, &disk,
        fake_read_sector, fake_write_sector) == ASTEROS_BLOCKDEV_OK);
    disk.fail_write = 1u;
    assert(asteros_blockdev_write(&device, 0u, sector) ==
           ASTEROS_BLOCKDEV_IO_ERROR);
}

static void test_invalid_volume_geometry_is_rejected(void) {
    fake_disk_t disk;
    asteros_volume_t volume = valid_volume();
    asteros_blockdev_t device;

    memset(&disk, 0, sizeof disk);
    assert(asteros_blockdev_init(&device, &volume, &disk,
        fake_read_sector, fake_write_sector) == ASTEROS_BLOCKDEV_OK);

    volume.data_start_lba++;
    assert(asteros_blockdev_init(&device, &volume, &disk,
        fake_read_sector, fake_write_sector) ==
        ASTEROS_BLOCKDEV_INVALID_VOLUME);
    assert(device.present == 0u);

    volume = valid_volume();
    volume.data_sector_count--;
    assert(asteros_blockdev_init(&device, &volume, &disk,
        fake_read_sector, fake_write_sector) ==
        ASTEROS_BLOCKDEV_INVALID_VOLUME);
    assert(device.present == 0u);

    volume = valid_volume();
    volume.start_lba = 0x0FFFFFF0u;
    volume.sector_count = 0x20u;
    volume.data_start_lba = volume.start_lba + 1u;
    volume.data_sector_count = volume.sector_count - 1u;
    assert(asteros_blockdev_init(&device, &volume, &disk,
        fake_read_sector, fake_write_sector) ==
        ASTEROS_BLOCKDEV_INVALID_VOLUME);

    volume = valid_volume();
    volume.present = 0u;
    assert(asteros_blockdev_init(&device, &volume, &disk,
        fake_read_sector, fake_write_sector) == ASTEROS_BLOCKDEV_NO_DEVICE);
    assert(device.present == 0u);
}

static void test_arguments_and_status_messages(void) {
    fake_disk_t disk;
    asteros_volume_t volume = valid_volume();
    asteros_blockdev_t device;
    uint8_t sector[ASTEROS_BLOCKDEV_SECTOR_SIZE];

    memset(&disk, 0, sizeof disk);
    assert(asteros_blockdev_init(0, &volume, &disk,
        fake_read_sector, fake_write_sector) ==
        ASTEROS_BLOCKDEV_INVALID_ARGUMENT);
    assert(asteros_blockdev_init(&device, 0, &disk,
        fake_read_sector, fake_write_sector) ==
        ASTEROS_BLOCKDEV_INVALID_ARGUMENT);
    assert(asteros_blockdev_init(&device, &volume, &disk, 0,
        fake_write_sector) == ASTEROS_BLOCKDEV_INVALID_ARGUMENT);
    assert(asteros_blockdev_read(0, 0u, sector) ==
           ASTEROS_BLOCKDEV_INVALID_ARGUMENT);
    assert(asteros_blockdev_read(&device, 0u, 0) ==
           ASTEROS_BLOCKDEV_INVALID_ARGUMENT);
    assert(strcmp(asteros_blockdev_status_message(ASTEROS_BLOCKDEV_OK),
                  "OK") == 0);
    assert(strcmp(asteros_blockdev_status_message(
        ASTEROS_BLOCKDEV_OUT_OF_RANGE),
        "Sector is outside the AsterOS data region") == 0);
}

int main(void) {
    test_relative_io_stays_inside_data_region();
    test_read_only_and_io_failures();
    test_invalid_volume_geometry_is_rejected();
    test_arguments_and_status_messages();
    puts("blockdev: tests passed");
    return 0;
}

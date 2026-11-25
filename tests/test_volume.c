#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "volume.h"

#define TEST_DISK_SECTORS 32768u
#define TEST_PARTITION_START 2048u
#define TEST_PARTITION_SECTORS (TEST_DISK_SECTORS - TEST_PARTITION_START)

typedef struct {
    uint8_t mbr[ASTEROS_VOLUME_SECTOR_SIZE];
    uint8_t header[ASTEROS_VOLUME_SECTOR_SIZE];
    uint32_t partition_start;
    uint32_t reads;
    uint8_t fail_mbr;
    uint8_t fail_header;
} fake_disk_t;

static void write_le16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
}

static void write_le32(uint8_t *bytes, uint32_t value) {
    bytes[0] = (uint8_t)value;
    bytes[1] = (uint8_t)(value >> 8);
    bytes[2] = (uint8_t)(value >> 16);
    bytes[3] = (uint8_t)(value >> 24);
}

static uint32_t test_crc32(const uint8_t *bytes, uint16_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    uint16_t index;
    for (index = 0; index < length; index++) {
        uint8_t bit;
        crc ^= bytes[index];
        for (bit = 0; bit < 8u; bit++)
            crc = (crc & 1u) ? ((crc >> 1) ^ 0xEDB88320u) : (crc >> 1);
    }
    return ~crc;
}

static void refresh_header_crc(fake_disk_t *disk) {
    write_le32(disk->header + 32u, test_crc32(disk->header, 32u));
}

static void set_partition(fake_disk_t *disk, uint8_t index, uint8_t type,
                          uint8_t boot_flag, uint32_t start, uint32_t count) {
    uint8_t *entry = disk->mbr + 446u + (uint16_t)index * 16u;
    entry[0] = boot_flag;
    entry[4] = type;
    write_le32(entry + 8u, start);
    write_le32(entry + 12u, count);
}

static void make_valid_disk(fake_disk_t *disk) {
    memset(disk, 0, sizeof *disk);
    disk->partition_start = TEST_PARTITION_START;
    disk->mbr[510] = 0x55u;
    disk->mbr[511] = 0xAAu;
    set_partition(disk, 0u, ASTEROS_VOLUME_PARTITION_TYPE, 0u,
                  TEST_PARTITION_START, TEST_PARTITION_SECTORS);

    memcpy(disk->header, "ASTEROS1", 8u);
    write_le16(disk->header + 8u, 1u);
    write_le16(disk->header + 10u, ASTEROS_VOLUME_HEADER_SIZE);
    write_le16(disk->header + 12u, ASTEROS_VOLUME_SECTOR_SIZE);
    write_le16(disk->header + 14u, 0u);
    write_le32(disk->header + 16u, TEST_PARTITION_START);
    write_le32(disk->header + 20u, TEST_PARTITION_SECTORS);
    write_le32(disk->header + 24u, ASTEROS_VOLUME_DATA_OFFSET_SECTORS);
    write_le32(disk->header + 28u, 0u);
    refresh_header_crc(disk);
}

static uint8_t fake_read_sector(void *context, uint32_t lba,
                                uint8_t *sector) {
    fake_disk_t *disk = (fake_disk_t *)context;
    disk->reads++;
    if (lba == 0u) {
        if (disk->fail_mbr) return 0u;
        memcpy(sector, disk->mbr, ASTEROS_VOLUME_SECTOR_SIZE);
        return 1u;
    }
    if (lba == disk->partition_start) {
        if (disk->fail_header) return 0u;
        memcpy(sector, disk->header, ASTEROS_VOLUME_SECTOR_SIZE);
        return 1u;
    }
    return 0u;
}

static asteros_volume_status_t probe(fake_disk_t *disk,
                                     asteros_volume_t *volume) {
    return asteros_volume_probe(fake_read_sector, disk, TEST_DISK_SECTORS,
                                volume);
}

static void assert_not_present(const asteros_volume_t *volume) {
    assert(volume->present == 0u);
    assert(volume->start_lba == 0u);
    assert(volume->sector_count == 0u);
    assert(volume->data_start_lba == 0u);
    assert(volume->data_sector_count == 0u);
}

static void test_valid_volume_and_read_only_probe(void) {
    assert(test_crc32((const uint8_t *)"123456789", 9u) == 0xCBF43926u);
    fake_disk_t disk;
    asteros_volume_t volume;

    make_valid_disk(&disk);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_OK);
    assert(volume.present == 1u);
    assert(volume.partition_index == 0u);
    assert(volume.start_lba == TEST_PARTITION_START);
    assert(volume.sector_count == TEST_PARTITION_SECTORS);
    assert(volume.data_start_lba == TEST_PARTITION_START + 1u);
    assert(volume.data_sector_count == TEST_PARTITION_SECTORS - 1u);
    assert(disk.reads == 2u);
}

static void test_absent_and_invalid_mbr(void) {
    fake_disk_t disk;
    asteros_volume_t volume;

    make_valid_disk(&disk);
    set_partition(&disk, 0u, 0x83u, 0u, TEST_PARTITION_START,
                  TEST_PARTITION_SECTORS);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_NO_VOLUME);
    assert_not_present(&volume);
    assert(disk.reads == 1u);

    make_valid_disk(&disk);
    disk.mbr[510] = 0u;
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_MBR);
    assert_not_present(&volume);
}

static void test_partition_bounds_and_ambiguity(void) {
    fake_disk_t disk;
    asteros_volume_t volume;

    make_valid_disk(&disk);
    disk.mbr[446] = 0x7Fu;
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_PARTITION);

    make_valid_disk(&disk);
    set_partition(&disk, 0u, ASTEROS_VOLUME_PARTITION_TYPE, 0u, 0u, 100u);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_PARTITION);

    make_valid_disk(&disk);
    set_partition(&disk, 0u, ASTEROS_VOLUME_PARTITION_TYPE, 0u,
                  TEST_PARTITION_START, 0u);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_PARTITION);

    make_valid_disk(&disk);
    set_partition(&disk, 0u, ASTEROS_VOLUME_PARTITION_TYPE, 0u,
                  TEST_DISK_SECTORS - 1u, 2u);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_PARTITION);

    make_valid_disk(&disk);
    set_partition(&disk, 0u, ASTEROS_VOLUME_PARTITION_TYPE, 0u,
                  TEST_PARTITION_START, 1u);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_PARTITION);

    make_valid_disk(&disk);
    set_partition(&disk, 1u, 0x83u, 0u,
                  TEST_PARTITION_START + 100u, 100u);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_PARTITION);

    make_valid_disk(&disk);
    set_partition(&disk, 1u, ASTEROS_VOLUME_PARTITION_TYPE, 0u,
                  TEST_PARTITION_START + 10000u, 100u);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_PARTITION);
    assert_not_present(&volume);
}

static void test_header_validation(void) {
    fake_disk_t disk;
    asteros_volume_t volume;

    make_valid_disk(&disk);
    disk.header[0] ^= 1u;
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_HEADER);

    make_valid_disk(&disk);
    write_le16(disk.header + 8u, 2u);
    refresh_header_crc(&disk);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_UNSUPPORTED_VERSION);

    make_valid_disk(&disk);
    write_le16(disk.header + 10u, 35u);
    refresh_header_crc(&disk);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_HEADER);

    make_valid_disk(&disk);
    write_le16(disk.header + 12u, 1024u);
    refresh_header_crc(&disk);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_HEADER);

    make_valid_disk(&disk);
    write_le16(disk.header + 14u, 1u);
    refresh_header_crc(&disk);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_HEADER);

    make_valid_disk(&disk);
    write_le32(disk.header + 16u, TEST_PARTITION_START + 1u);
    refresh_header_crc(&disk);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_HEADER);

    make_valid_disk(&disk);
    write_le32(disk.header + 20u, TEST_PARTITION_SECTORS - 1u);
    refresh_header_crc(&disk);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_HEADER);

    make_valid_disk(&disk);
    write_le32(disk.header + 24u, 2u);
    refresh_header_crc(&disk);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_HEADER);

    make_valid_disk(&disk);
    write_le32(disk.header + 28u, 1u);
    refresh_header_crc(&disk);
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_HEADER);

    make_valid_disk(&disk);
    disk.header[32] ^= 1u;
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_HEADER);

    make_valid_disk(&disk);
    disk.header[100] = 1u;
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_INVALID_HEADER);
}

static void test_io_and_arguments(void) {
    fake_disk_t disk;
    asteros_volume_t volume;

    make_valid_disk(&disk);
    disk.fail_mbr = 1u;
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_IO_ERROR);
    assert_not_present(&volume);

    make_valid_disk(&disk);
    disk.fail_header = 1u;
    assert(probe(&disk, &volume) == ASTEROS_VOLUME_IO_ERROR);
    assert_not_present(&volume);

    make_valid_disk(&disk);
    assert(asteros_volume_probe(0, &disk, TEST_DISK_SECTORS, &volume) ==
           ASTEROS_VOLUME_INVALID_ARGUMENT);
    assert(asteros_volume_probe(fake_read_sector, &disk, 0u, &volume) ==
           ASTEROS_VOLUME_INVALID_ARGUMENT);
    assert(asteros_volume_probe(fake_read_sector, &disk, 0x10000001u,
                                &volume) == ASTEROS_VOLUME_INVALID_ARGUMENT);
    assert(asteros_volume_probe(fake_read_sector, &disk, TEST_DISK_SECTORS,
                                0) == ASTEROS_VOLUME_INVALID_ARGUMENT);
    assert(strcmp(asteros_volume_status_message(ASTEROS_VOLUME_OK),
                  "AsterOS volume found") == 0);
}

int main(void) {
    test_valid_volume_and_read_only_probe();
    test_absent_and_invalid_mbr();
    test_partition_bounds_and_ambiguity();
    test_header_validation();
    test_io_and_arguments();
    puts("volume discovery: tests passed");
    return 0;
}

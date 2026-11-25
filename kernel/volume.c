#include "volume.h"

enum {
    MBR_PARTITION_TABLE_OFFSET = 446u,
    MBR_PARTITION_ENTRY_SIZE = 16u,
    MBR_SIGNATURE_OFFSET = 510u,
    MBR_PARTITION_COUNT = 4u,
    VOLUME_VERSION = 1u,
    VOLUME_CRC_INPUT_SIZE = 32u,
    VOLUME_CRC_OFFSET = 32u,
    LBA28_SECTOR_LIMIT = 0x10000000u
};

static const uint8_t volume_magic[8] = {
    'A', 'S', 'T', 'E', 'R', 'O', 'S', '1'
};

static uint16_t read_le16(const uint8_t *bytes) {
    return (uint16_t)((uint16_t)bytes[0] |
                      ((uint16_t)bytes[1] << 8));
}

static uint32_t read_le32(const uint8_t *bytes) {
    return (uint32_t)bytes[0] |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static uint8_t bytes_equal(const uint8_t *left, const uint8_t *right,
                           uint8_t length) {
    uint8_t index;
    for (index = 0; index < length; index++) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static uint32_t crc32(const uint8_t *bytes, uint16_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    uint16_t index;

    for (index = 0; index < length; index++) {
        uint8_t bit;
        crc ^= bytes[index];
        for (bit = 0; bit < 8u; bit++) {
            if (crc & 1u) crc = (crc >> 1) ^ 0xEDB88320u;
            else crc >>= 1;
        }
    }
    return ~crc;
}

static void clear_volume(asteros_volume_t *volume) {
    if (!volume) return;
    volume->present = 0u;
    volume->partition_index = 0u;
    volume->start_lba = 0u;
    volume->sector_count = 0u;
    volume->data_start_lba = 0u;
    volume->data_sector_count = 0u;
}

static asteros_volume_status_t validate_header(
    const uint8_t *header, uint32_t partition_start,
    uint32_t partition_sectors) {
    uint16_t index;

    if (!bytes_equal(header, volume_magic, sizeof volume_magic))
        return ASTEROS_VOLUME_INVALID_HEADER;
    if (read_le16(header + 8u) != VOLUME_VERSION)
        return ASTEROS_VOLUME_UNSUPPORTED_VERSION;
    if (read_le16(header + 10u) != ASTEROS_VOLUME_HEADER_SIZE ||
        read_le16(header + 12u) != ASTEROS_VOLUME_SECTOR_SIZE ||
        read_le16(header + 14u) != 0u ||
        read_le32(header + 16u) != partition_start ||
        read_le32(header + 20u) != partition_sectors ||
        read_le32(header + 24u) != ASTEROS_VOLUME_DATA_OFFSET_SECTORS ||
        read_le32(header + 28u) != 0u ||
        read_le32(header + VOLUME_CRC_OFFSET) !=
            crc32(header, VOLUME_CRC_INPUT_SIZE))
        return ASTEROS_VOLUME_INVALID_HEADER;

    /* Version 1 reserves the rest of the descriptor sector. Requiring zeroes
       keeps future format extensions explicit instead of silently accepting
       unknown metadata as if it were understood. */
    for (index = ASTEROS_VOLUME_HEADER_SIZE;
         index < ASTEROS_VOLUME_SECTOR_SIZE; index++) {
        if (header[index] != 0u) return ASTEROS_VOLUME_INVALID_HEADER;
    }
    return ASTEROS_VOLUME_OK;
}

asteros_volume_status_t asteros_volume_probe(
    asteros_volume_read_sector_fn read_sector, void *context,
    uint32_t disk_sectors, asteros_volume_t *volume) {
    uint8_t mbr[ASTEROS_VOLUME_SECTOR_SIZE];
    uint8_t header[ASTEROS_VOLUME_SECTOR_SIZE];
    uint32_t partition_start[MBR_PARTITION_COUNT];
    uint32_t partition_sectors[MBR_PARTITION_COUNT];
    uint8_t partition_types[MBR_PARTITION_COUNT];
    uint8_t candidate = 0xFFu;
    uint8_t index;
    uint32_t candidate_end;
    asteros_volume_status_t status;

    clear_volume(volume);
    if (!read_sector || !volume || disk_sectors < 2u ||
        disk_sectors > LBA28_SECTOR_LIMIT)
        return ASTEROS_VOLUME_INVALID_ARGUMENT;
    if (!read_sector(context, 0u, mbr)) return ASTEROS_VOLUME_IO_ERROR;
    if (mbr[MBR_SIGNATURE_OFFSET] != 0x55u ||
        mbr[MBR_SIGNATURE_OFFSET + 1u] != 0xAAu)
        return ASTEROS_VOLUME_INVALID_MBR;

    for (index = 0; index < MBR_PARTITION_COUNT; index++) {
        const uint8_t *entry = mbr + MBR_PARTITION_TABLE_OFFSET +
                               (uint16_t)index * MBR_PARTITION_ENTRY_SIZE;
        uint8_t boot_flag = entry[0];
        uint8_t type = entry[4];
        uint32_t start = read_le32(entry + 8u);
        uint32_t count = read_le32(entry + 12u);

        partition_types[index] = type;
        partition_start[index] = start;
        partition_sectors[index] = count;

        if (boot_flag != 0u && boot_flag != 0x80u)
            return ASTEROS_VOLUME_INVALID_PARTITION;
        if (type == 0u) {
            if (boot_flag != 0u) return ASTEROS_VOLUME_INVALID_PARTITION;
            continue;
        }
        if (start == 0u || count == 0u || start >= disk_sectors ||
            count > disk_sectors - start ||
            start >= LBA28_SECTOR_LIMIT ||
            count > LBA28_SECTOR_LIMIT - start)
            return ASTEROS_VOLUME_INVALID_PARTITION;

        if (type == ASTEROS_VOLUME_PARTITION_TYPE) {
            if (candidate != 0xFFu)
                return ASTEROS_VOLUME_INVALID_PARTITION;
            candidate = index;
        }
    }

    if (candidate == 0xFFu) return ASTEROS_VOLUME_NO_VOLUME;
    if (partition_sectors[candidate] < 2u)
        return ASTEROS_VOLUME_INVALID_PARTITION;

    candidate_end = partition_start[candidate] +
                    partition_sectors[candidate];
    for (index = 0; index < MBR_PARTITION_COUNT; index++) {
        uint32_t other_end;
        if (index == candidate || partition_types[index] == 0u) continue;
        other_end = partition_start[index] + partition_sectors[index];
        if (partition_start[candidate] < other_end &&
            partition_start[index] < candidate_end)
            return ASTEROS_VOLUME_INVALID_PARTITION;
    }

    if (!read_sector(context, partition_start[candidate], header))
        return ASTEROS_VOLUME_IO_ERROR;
    status = validate_header(header, partition_start[candidate],
                             partition_sectors[candidate]);
    if (status != ASTEROS_VOLUME_OK) return status;

    volume->present = 1u;
    volume->partition_index = candidate;
    volume->start_lba = partition_start[candidate];
    volume->sector_count = partition_sectors[candidate];
    volume->data_start_lba = partition_start[candidate] +
                             ASTEROS_VOLUME_DATA_OFFSET_SECTORS;
    volume->data_sector_count = partition_sectors[candidate] -
                                ASTEROS_VOLUME_DATA_OFFSET_SECTORS;
    return ASTEROS_VOLUME_OK;
}

const char *asteros_volume_status_message(asteros_volume_status_t status) {
    switch (status) {
        case ASTEROS_VOLUME_OK: return "AsterOS volume found";
        case ASTEROS_VOLUME_INVALID_ARGUMENT: return "Invalid volume probe";
        case ASTEROS_VOLUME_NO_VOLUME: return "No AsterOS volume";
        case ASTEROS_VOLUME_IO_ERROR: return "Unable to read AsterOS volume";
        case ASTEROS_VOLUME_INVALID_MBR: return "Invalid MBR partition table";
        case ASTEROS_VOLUME_INVALID_PARTITION: return "Invalid AsterOS partition";
        case ASTEROS_VOLUME_UNSUPPORTED_VERSION: return "Unsupported AsterOS volume version";
        case ASTEROS_VOLUME_INVALID_HEADER: return "Invalid AsterOS volume descriptor";
        default: return "Unknown AsterOS volume error";
    }
}

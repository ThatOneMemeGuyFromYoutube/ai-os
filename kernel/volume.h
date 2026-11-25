#ifndef ASTEROS_VOLUME_H
#define ASTEROS_VOLUME_H

#include <stdint.h>

#define ASTEROS_VOLUME_SECTOR_SIZE 512u
#define ASTEROS_VOLUME_PARTITION_TYPE 0x7Fu
#define ASTEROS_VOLUME_HEADER_SIZE 36u
#define ASTEROS_VOLUME_DATA_OFFSET_SECTORS 1u

typedef enum {
    ASTEROS_VOLUME_OK = 0,
    ASTEROS_VOLUME_INVALID_ARGUMENT,
    ASTEROS_VOLUME_NO_VOLUME,
    ASTEROS_VOLUME_IO_ERROR,
    ASTEROS_VOLUME_INVALID_MBR,
    ASTEROS_VOLUME_INVALID_PARTITION,
    ASTEROS_VOLUME_UNSUPPORTED_VERSION,
    ASTEROS_VOLUME_INVALID_HEADER
} asteros_volume_status_t;

typedef uint8_t (*asteros_volume_read_sector_fn)(
    void *context, uint32_t lba, uint8_t *sector);

typedef struct {
    uint8_t present;
    uint8_t partition_index;
    uint32_t start_lba;
    uint32_t sector_count;
    uint32_t data_start_lba;
    uint32_t data_sector_count;
} asteros_volume_t;

/* Read-only discovery for the first AsterOS volume format. The MBR must
   contain exactly one non-overlapping primary partition of type 0x7F and
   that partition must begin with a valid ASTEROS1 v1 descriptor. */
asteros_volume_status_t asteros_volume_probe(
    asteros_volume_read_sector_fn read_sector, void *context,
    uint32_t disk_sectors, asteros_volume_t *volume);

const char *asteros_volume_status_message(asteros_volume_status_t status);

#endif

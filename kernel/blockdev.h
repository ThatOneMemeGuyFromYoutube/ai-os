#ifndef ASTEROS_BLOCKDEV_H
#define ASTEROS_BLOCKDEV_H

#include <stdint.h>
#include "volume.h"

#define ASTEROS_BLOCKDEV_SECTOR_SIZE ASTEROS_VOLUME_SECTOR_SIZE

typedef enum {
    ASTEROS_BLOCKDEV_OK = 0,
    ASTEROS_BLOCKDEV_INVALID_ARGUMENT,
    ASTEROS_BLOCKDEV_NO_DEVICE,
    ASTEROS_BLOCKDEV_INVALID_VOLUME,
    ASTEROS_BLOCKDEV_READ_ONLY,
    ASTEROS_BLOCKDEV_OUT_OF_RANGE,
    ASTEROS_BLOCKDEV_IO_ERROR
} asteros_blockdev_status_t;

/* Callbacks use absolute disk LBAs. A successful write callback must not
   return until the sector write and device cache flush have completed. */
typedef uint8_t (*asteros_blockdev_read_fn)(
    void *context, uint32_t lba, uint8_t *sector);
typedef uint8_t (*asteros_blockdev_write_fn)(
    void *context, uint32_t lba, const uint8_t *sector);

typedef struct {
    void *context;
    asteros_blockdev_read_fn read_sector;
    asteros_blockdev_write_fn write_sector;
    uint32_t start_lba;
    uint32_t sector_count;
    uint8_t present;
} asteros_blockdev_t;

/* Expose only the validated ASTEROS1 data region. A null write callback
   creates a read-only device. The MBR and volume descriptor are never in the
   relative address space. */
asteros_blockdev_status_t asteros_blockdev_init(
    asteros_blockdev_t *device, const asteros_volume_t *volume,
    void *context, asteros_blockdev_read_fn read_sector,
    asteros_blockdev_write_fn write_sector);

asteros_blockdev_status_t asteros_blockdev_read(
    const asteros_blockdev_t *device, uint32_t relative_lba,
    uint8_t *sector);
asteros_blockdev_status_t asteros_blockdev_write(
    const asteros_blockdev_t *device, uint32_t relative_lba,
    const uint8_t *sector);

const char *asteros_blockdev_status_message(
    asteros_blockdev_status_t status);

#endif

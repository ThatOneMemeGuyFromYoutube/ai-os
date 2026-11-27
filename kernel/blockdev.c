#include "blockdev.h"

#define ASTEROS_BLOCKDEV_LBA28_LIMIT 0x10000000u

static void clear_device(asteros_blockdev_t *device) {
    if (!device) return;
    device->context = 0;
    device->read_sector = 0;
    device->write_sector = 0;
    device->start_lba = 0;
    device->sector_count = 0;
    device->present = 0;
}

asteros_blockdev_status_t asteros_blockdev_init(
    asteros_blockdev_t *device, const asteros_volume_t *volume,
    void *context, asteros_blockdev_read_fn read_sector,
    asteros_blockdev_write_fn write_sector) {
    if (!device) return ASTEROS_BLOCKDEV_INVALID_ARGUMENT;
    clear_device(device);

    if (!volume || !read_sector)
        return ASTEROS_BLOCKDEV_INVALID_ARGUMENT;
    if (!volume->present) return ASTEROS_BLOCKDEV_NO_DEVICE;

    /* Recheck the descriptor geometry at this boundary. The block device
       must never expose LBA 0 or the sector containing ASTEROS1 metadata. */
    if (volume->partition_index >= 4u ||
        volume->start_lba == 0u ||
        volume->sector_count < 2u ||
        volume->start_lba >= ASTEROS_BLOCKDEV_LBA28_LIMIT ||
        volume->sector_count >
            ASTEROS_BLOCKDEV_LBA28_LIMIT - volume->start_lba ||
        volume->data_start_lba != volume->start_lba + 1u ||
        volume->data_sector_count != volume->sector_count - 1u ||
        volume->data_start_lba >= ASTEROS_BLOCKDEV_LBA28_LIMIT ||
        volume->data_sector_count >
            ASTEROS_BLOCKDEV_LBA28_LIMIT - volume->data_start_lba)
        return ASTEROS_BLOCKDEV_INVALID_VOLUME;

    device->context = context;
    device->read_sector = read_sector;
    device->write_sector = write_sector;
    device->start_lba = volume->data_start_lba;
    device->sector_count = volume->data_sector_count;
    device->present = 1u;
    return ASTEROS_BLOCKDEV_OK;
}

static asteros_blockdev_status_t validate_transfer(
    const asteros_blockdev_t *device, uint32_t relative_lba,
    const uint8_t *sector) {
    if (!device || !sector) return ASTEROS_BLOCKDEV_INVALID_ARGUMENT;
    if (!device->present) return ASTEROS_BLOCKDEV_NO_DEVICE;
    if (!device->read_sector) return ASTEROS_BLOCKDEV_INVALID_ARGUMENT;
    if (device->start_lba >= ASTEROS_BLOCKDEV_LBA28_LIMIT ||
        device->sector_count >
            ASTEROS_BLOCKDEV_LBA28_LIMIT - device->start_lba ||
        relative_lba >= device->sector_count ||
        relative_lba >
            (ASTEROS_BLOCKDEV_LBA28_LIMIT - 1u) - device->start_lba)
        return ASTEROS_BLOCKDEV_OUT_OF_RANGE;
    return ASTEROS_BLOCKDEV_OK;
}

asteros_blockdev_status_t asteros_blockdev_read(
    const asteros_blockdev_t *device, uint32_t relative_lba,
    uint8_t *sector) {
    asteros_blockdev_status_t status =
        validate_transfer(device, relative_lba, sector);
    if (status != ASTEROS_BLOCKDEV_OK) return status;
    if (!device->read_sector(device->context,
                             device->start_lba + relative_lba, sector))
        return ASTEROS_BLOCKDEV_IO_ERROR;
    return ASTEROS_BLOCKDEV_OK;
}

asteros_blockdev_status_t asteros_blockdev_write(
    const asteros_blockdev_t *device, uint32_t relative_lba,
    const uint8_t *sector) {
    asteros_blockdev_status_t status =
        validate_transfer(device, relative_lba, sector);
    if (status != ASTEROS_BLOCKDEV_OK) return status;
    if (!device->write_sector) return ASTEROS_BLOCKDEV_READ_ONLY;
    if (!device->write_sector(device->context,
                              device->start_lba + relative_lba, sector))
        return ASTEROS_BLOCKDEV_IO_ERROR;
    return ASTEROS_BLOCKDEV_OK;
}

const char *asteros_blockdev_status_message(
    asteros_blockdev_status_t status) {
    switch (status) {
        case ASTEROS_BLOCKDEV_OK: return "OK";
        case ASTEROS_BLOCKDEV_INVALID_ARGUMENT:
            return "Invalid block-device request";
        case ASTEROS_BLOCKDEV_NO_DEVICE:
            return "No validated AsterOS volume";
        case ASTEROS_BLOCKDEV_INVALID_VOLUME:
            return "Invalid AsterOS block-device geometry";
        case ASTEROS_BLOCKDEV_READ_ONLY:
            return "AsterOS block device is read-only";
        case ASTEROS_BLOCKDEV_OUT_OF_RANGE:
            return "Sector is outside the AsterOS data region";
        case ASTEROS_BLOCKDEV_IO_ERROR:
            return "AsterOS block-device I/O failed";
        default: return "Unknown block-device error";
    }
}

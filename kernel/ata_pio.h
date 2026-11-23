#ifndef ASTEROS_ATA_PIO_H
#define ASTEROS_ATA_PIO_H

#include <stdint.h>

#define ASTEROS_ATA_SECTOR_SIZE 512u
#define ASTEROS_ATA_PIO_POLL_LIMIT 100000u

typedef enum {
    ASTEROS_ATA_OK = 0,
    ASTEROS_ATA_INVALID_ARGUMENT,
    ASTEROS_ATA_NO_DEVICE,
    ASTEROS_ATA_TIMEOUT,
    ASTEROS_ATA_DEVICE_ERROR,
    ASTEROS_ATA_UNSUPPORTED,
    ASTEROS_ATA_OUT_OF_RANGE
} asteros_ata_status_t;

typedef struct {
    void *context;
    uint8_t (*in8)(void *context, uint16_t port);
    uint16_t (*in16)(void *context, uint16_t port);
    void (*out8)(void *context, uint16_t port, uint8_t value);
    void (*out16)(void *context, uint16_t port, uint16_t value);
} asteros_ata_io_t;

typedef struct {
    const asteros_ata_io_t *io;
    uint16_t io_base;
    uint16_t control_base;
    uint32_t sectors;
    uint8_t drive;
    uint8_t present;
} asteros_ata_device_t;

/* Probe one ATA device using IDENTIFY DEVICE. Only LBA28-capable ATA disks
   are accepted; ATAPI devices and devices without LBA support are rejected. */
asteros_ata_status_t asteros_ata_pio_probe(
    asteros_ata_device_t *device, const asteros_ata_io_t *io,
    uint16_t io_base, uint16_t control_base, uint8_t drive);

/* Transfer exactly one 512-byte LBA28 sector. No DMA or IRQ path is used.
   Writes issue and wait for CACHE FLUSH before reporting success. */
asteros_ata_status_t asteros_ata_pio_read(
    const asteros_ata_device_t *device, uint32_t lba, uint8_t *sector);
asteros_ata_status_t asteros_ata_pio_write(
    const asteros_ata_device_t *device, uint32_t lba,
    const uint8_t *sector);

const char *asteros_ata_status_message(asteros_ata_status_t status);

#endif

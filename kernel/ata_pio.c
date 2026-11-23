#include "ata_pio.h"

enum {
    ATA_REG_DATA = 0,
    ATA_REG_SECTOR_COUNT = 2,
    ATA_REG_LBA_LOW = 3,
    ATA_REG_LBA_MID = 4,
    ATA_REG_LBA_HIGH = 5,
    ATA_REG_DRIVE = 6,
    ATA_REG_STATUS_COMMAND = 7,
    ATA_STATUS_ERR = 0x01,
    ATA_STATUS_DEVICE_FAULT = 0x20,
    ATA_STATUS_DRQ = 0x08,
    ATA_STATUS_BUSY = 0x80,
    ATA_COMMAND_READ = 0x20,
    ATA_COMMAND_WRITE = 0x30,
    ATA_COMMAND_IDENTIFY = 0xEC,
    ATA_COMMAND_CACHE_FLUSH = 0xE7,
    ATA_CONTROL_DISABLE_INTERRUPTS = 0x02,
    ATA_IDENTIFY_LBA = 49,
    ATA_IDENTIFY_LBA28_SECTORS = 60
};

static uint8_t valid_io(const asteros_ata_io_t *io) {
    return (uint8_t)(io && io->in8 && io->in16 && io->out8 && io->out16);
}

static uint8_t read_status(const asteros_ata_device_t *device) {
    return device->io->in8(device->io->context,
                           (uint16_t)(device->io_base + ATA_REG_STATUS_COMMAND));
}

static void io_delay(const asteros_ata_device_t *device) {
    uint8_t count;
    for (count = 0; count < 4u; count++)
        (void)device->io->in8(device->io->context, device->control_base);
}

static asteros_ata_status_t wait_not_busy(
    const asteros_ata_device_t *device) {
    uint32_t attempt;
    for (attempt = 0; attempt < ASTEROS_ATA_PIO_POLL_LIMIT; attempt++) {
        uint8_t status = read_status(device);
        if (status == 0u || status == 0xFFu) return ASTEROS_ATA_NO_DEVICE;
        if (status & ATA_STATUS_BUSY) continue;
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DEVICE_FAULT))
            return ASTEROS_ATA_DEVICE_ERROR;
        return ASTEROS_ATA_OK;
    }
    return ASTEROS_ATA_TIMEOUT;
}

static asteros_ata_status_t wait_data_request(
    const asteros_ata_device_t *device) {
    uint32_t attempt;
    for (attempt = 0; attempt < ASTEROS_ATA_PIO_POLL_LIMIT; attempt++) {
        uint8_t status = read_status(device);
        if (status == 0u || status == 0xFFu) return ASTEROS_ATA_NO_DEVICE;
        if (status & ATA_STATUS_BUSY) continue;
        if (status & (ATA_STATUS_ERR | ATA_STATUS_DEVICE_FAULT))
            return ASTEROS_ATA_DEVICE_ERROR;
        if (status & ATA_STATUS_DRQ) return ASTEROS_ATA_OK;
    }
    return ASTEROS_ATA_TIMEOUT;
}

static void select_drive(const asteros_ata_device_t *device,
                         uint8_t lba_high_nibble) {
    uint8_t value = (uint8_t)(0xE0u | (device->drive << 4) |
                              (lba_high_nibble & 0x0Fu));
    device->io->out8(device->io->context,
                     (uint16_t)(device->io_base + ATA_REG_DRIVE), value);
    io_delay(device);
}

static asteros_ata_status_t begin_sector(
    const asteros_ata_device_t *device, uint32_t lba, uint8_t command) {
    asteros_ata_status_t status = wait_not_busy(device);
    if (status != ASTEROS_ATA_OK) return status;

    select_drive(device, (uint8_t)(lba >> 24));
    status = wait_not_busy(device);
    if (status != ASTEROS_ATA_OK) return status;
    device->io->out8(device->io->context,
        (uint16_t)(device->io_base + ATA_REG_SECTOR_COUNT), 1u);
    device->io->out8(device->io->context,
        (uint16_t)(device->io_base + ATA_REG_LBA_LOW), (uint8_t)lba);
    device->io->out8(device->io->context,
        (uint16_t)(device->io_base + ATA_REG_LBA_MID), (uint8_t)(lba >> 8));
    device->io->out8(device->io->context,
        (uint16_t)(device->io_base + ATA_REG_LBA_HIGH), (uint8_t)(lba >> 16));
    device->io->out8(device->io->context,
        (uint16_t)(device->io_base + ATA_REG_STATUS_COMMAND), command);
    io_delay(device);
    return wait_data_request(device);
}

asteros_ata_status_t asteros_ata_pio_probe(
    asteros_ata_device_t *device, const asteros_ata_io_t *io,
    uint16_t io_base, uint16_t control_base, uint8_t drive) {
    uint16_t identify[256];
    uint16_t index;
    uint8_t status;

    if (!device || !valid_io(io) || !io_base || io_base > 0xFFF8u || !control_base || drive > 1u)
        return ASTEROS_ATA_INVALID_ARGUMENT;

    device->io = io;
    device->io_base = io_base;
    device->control_base = control_base;
    device->drive = drive;
    device->sectors = 0;
    device->present = 0;

    /* The kernel polls synchronously with interrupts disabled. Do not allow
       this channel to raise an unhandled ATA interrupt. */
    io->out8(io->context, control_base, ATA_CONTROL_DISABLE_INTERRUPTS);
    io->out8(io->context, (uint16_t)(io_base + ATA_REG_DRIVE),
             (uint8_t)(0xA0u | (drive << 4)));
    io_delay(device);

    {
        asteros_ata_status_t result = wait_not_busy(device);
        if (result != ASTEROS_ATA_OK) return result;
    }

    io->out8(io->context, (uint16_t)(io_base + ATA_REG_SECTOR_COUNT), 0);
    io->out8(io->context, (uint16_t)(io_base + ATA_REG_LBA_LOW), 0);
    io->out8(io->context, (uint16_t)(io_base + ATA_REG_LBA_MID), 0);
    io->out8(io->context, (uint16_t)(io_base + ATA_REG_LBA_HIGH), 0);
    io->out8(io->context, (uint16_t)(io_base + ATA_REG_STATUS_COMMAND),
             ATA_COMMAND_IDENTIFY);
    io_delay(device);

    status = read_status(device);
    if (status == 0u || status == 0xFFu) return ASTEROS_ATA_NO_DEVICE;
    {
        asteros_ata_status_t result = wait_not_busy(device);
        if (result != ASTEROS_ATA_OK) return result;
    }

    /* IDENTIFY DEVICE returns zero in these registers. Nonzero values identify
       an ATAPI packet device, which this sector driver deliberately rejects. */
    if (io->in8(io->context, (uint16_t)(io_base + ATA_REG_LBA_MID)) != 0u ||
        io->in8(io->context, (uint16_t)(io_base + ATA_REG_LBA_HIGH)) != 0u)
        return ASTEROS_ATA_UNSUPPORTED;

    {
        asteros_ata_status_t result = wait_data_request(device);
        if (result != ASTEROS_ATA_OK) return result;
    }
    for (index = 0; index < 256u; index++)
        identify[index] = io->in16(io->context, io_base);

    if (!(identify[ATA_IDENTIFY_LBA] & 0x0200u))
        return ASTEROS_ATA_UNSUPPORTED;

    device->sectors = (uint32_t)identify[ATA_IDENTIFY_LBA28_SECTORS] |
                      ((uint32_t)identify[ATA_IDENTIFY_LBA28_SECTORS + 1u] << 16);
    if (!device->sectors) return ASTEROS_ATA_UNSUPPORTED;
    device->present = 1;
    return ASTEROS_ATA_OK;
}

static asteros_ata_status_t validate_transfer(
    const asteros_ata_device_t *device, uint32_t lba, const uint8_t *buffer) {
    if (!device || !buffer || !device->io || !valid_io(device->io))
        return ASTEROS_ATA_INVALID_ARGUMENT;
    if (!device->present) return ASTEROS_ATA_NO_DEVICE;
    if (lba >= device->sectors || lba > 0x0FFFFFFFu)
        return ASTEROS_ATA_OUT_OF_RANGE;
    return ASTEROS_ATA_OK;
}

asteros_ata_status_t asteros_ata_pio_read(
    const asteros_ata_device_t *device, uint32_t lba, uint8_t *sector) {
    asteros_ata_status_t status = validate_transfer(device, lba, sector);
    uint16_t index;
    if (status != ASTEROS_ATA_OK) return status;
    status = begin_sector(device, lba, ATA_COMMAND_READ);
    if (status != ASTEROS_ATA_OK) return status;
    for (index = 0; index < 256u; index++) {
        uint16_t word = device->io->in16(device->io->context, device->io_base);
        sector[index * 2u] = (uint8_t)word;
        sector[index * 2u + 1u] = (uint8_t)(word >> 8);
    }
    return wait_not_busy(device);
}

asteros_ata_status_t asteros_ata_pio_write(
    const asteros_ata_device_t *device, uint32_t lba, const uint8_t *sector) {
    asteros_ata_status_t status = validate_transfer(device, lba, sector);
    uint16_t index;
    if (status != ASTEROS_ATA_OK) return status;
    status = begin_sector(device, lba, ATA_COMMAND_WRITE);
    if (status != ASTEROS_ATA_OK) return status;
    for (index = 0; index < 256u; index++) {
        uint16_t word = (uint16_t)sector[index * 2u] |
                        ((uint16_t)sector[index * 2u + 1u] << 8);
        device->io->out16(device->io->context, device->io_base, word);
    }
    status = wait_not_busy(device);
    if (status != ASTEROS_ATA_OK) return status;

    device->io->out8(device->io->context,
        (uint16_t)(device->io_base + ATA_REG_STATUS_COMMAND),
        ATA_COMMAND_CACHE_FLUSH);
    io_delay(device);
    return wait_not_busy(device);
}

const char *asteros_ata_status_message(asteros_ata_status_t status) {
    switch (status) {
        case ASTEROS_ATA_OK: return "OK";
        case ASTEROS_ATA_INVALID_ARGUMENT: return "Invalid ATA request";
        case ASTEROS_ATA_NO_DEVICE: return "No ATA device";
        case ASTEROS_ATA_TIMEOUT: return "ATA device timed out";
        case ASTEROS_ATA_DEVICE_ERROR: return "ATA device reported an error";
        case ASTEROS_ATA_UNSUPPORTED: return "ATA device lacks supported LBA28 mode";
        case ASTEROS_ATA_OUT_OF_RANGE: return "ATA sector is out of range";
        default: return "Unknown ATA error";
    }
}

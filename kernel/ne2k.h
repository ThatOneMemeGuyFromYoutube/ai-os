#ifndef ASTEROS_NE2K_H
#define ASTEROS_NE2K_H

#include <stdint.h>
#include "net.h"
#include "pci.h"

#define ASTEROS_NE2K_VENDOR_REALTEK 0x10ECu
#define ASTEROS_NE2K_DEVICE_RTL8029 0x8029u
#define ASTEROS_NE2K_FRAME_MAX (ASTEROS_ETHERNET_MTU_MAX + ASTEROS_ETHERNET_HEADER_SIZE)

typedef struct {
    uint8_t (*read8)(void *context, uint16_t port);
    void (*write8)(void *context, uint16_t port, uint8_t value);
    uint16_t (*read16)(void *context, uint16_t port);
    void (*write16)(void *context, uint16_t port, uint16_t value);
    uint8_t (*enable_io)(void *context, const asteros_pci_device_t *device);
} asteros_ne2k_ops_t;

typedef enum {
    ASTEROS_NE2K_OK = 0,
    ASTEROS_NE2K_INVALID_ARGUMENT,
    ASTEROS_NE2K_UNSUPPORTED_DEVICE,
    ASTEROS_NE2K_INVALID_BAR,
    ASTEROS_NE2K_PCI_ENABLE_FAILED,
    ASTEROS_NE2K_RESET_TIMEOUT,
    ASTEROS_NE2K_DMA_TIMEOUT,
    ASTEROS_NE2K_INVALID_MAC,
    ASTEROS_NE2K_NET_REGISTER_FAILED
} asteros_ne2k_result_t;

typedef enum {
    ASTEROS_NE2K_POLL_NONE = 0,
    ASTEROS_NE2K_POLL_FRAME,
    ASTEROS_NE2K_POLL_DROPPED,
    ASTEROS_NE2K_POLL_ERROR
} asteros_ne2k_poll_result_t;

/* A polling, programmed-I/O RTL8029AS driver. It does not use DMA bus
   mastering or interrupts; packet buffers remain owned by this object. */
typedef struct {
    asteros_netif_t interface;
    asteros_ne2k_ops_t ops;
    void *io_context;
    uint16_t io_base;
    uint8_t initialized;
    uint8_t receive_buffer[ASTEROS_NE2K_FRAME_MAX];
    uint8_t transmit_buffer[ASTEROS_NE2K_FRAME_MAX];
} asteros_ne2k_driver_t;

/* Validate the PCI function and assigned BAR0 before enabling I/O decoding.
   The enable callback is called only after the resource has passed validation. */
asteros_ne2k_result_t asteros_ne2k_start(
    asteros_ne2k_driver_t *driver, const asteros_pci_device_t *device,
    const asteros_pci_resources_t *resources,
    const asteros_ne2k_ops_t *ops, void *io_context);

void asteros_ne2k_stop(asteros_ne2k_driver_t *driver);

/* Process at most one pending receive-ring packet. This is intended to be
   called frequently from the current polling kernel loop. */
asteros_ne2k_poll_result_t asteros_ne2k_poll(
    asteros_ne2k_driver_t *driver);

/* Kernel-facing wrappers use PCI configuration mechanism #1 and x86 port I/O. */
void asteros_ne2k_init_default(void);
void asteros_ne2k_poll_default(void);

#endif

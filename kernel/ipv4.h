#ifndef ASTEROS_IPV4_H
#define ASTEROS_IPV4_H
#include <stdint.h>
#include "net.h"

#define ASTEROS_IPV4_ETHERTYPE 0x0800u
#define ASTEROS_IPV4_HEADER_MIN 20u
#define ASTEROS_IPV4_HEADER_MAX 60u
#define ASTEROS_IPV4_MAX_PROTOCOL_HANDLERS 8u

typedef enum {
    ASTEROS_IPV4_OK = 0,
    ASTEROS_IPV4_INVALID_ARGUMENT,
    ASTEROS_IPV4_ALREADY_INITIALIZED,
    ASTEROS_IPV4_NOT_INITIALIZED,
    ASTEROS_IPV4_INTERFACE_NOT_REGISTERED,
    ASTEROS_IPV4_INTERFACE_NOT_CONFIGURED,
    ASTEROS_IPV4_INVALID_ADDRESS,
    ASTEROS_IPV4_ADDRESS_IN_USE,
    ASTEROS_IPV4_TABLE_FULL,
    ASTEROS_IPV4_HANDLER_REGISTRATION_FAILED,
    ASTEROS_IPV4_PACKET_TOO_SHORT,
    ASTEROS_IPV4_INVALID_SOURCE,
    ASTEROS_IPV4_WRONG_DESTINATION,
    ASTEROS_IPV4_INVALID_VERSION,
    ASTEROS_IPV4_INVALID_HEADER_LENGTH,
    ASTEROS_IPV4_INVALID_TOTAL_LENGTH,
    ASTEROS_IPV4_INVALID_CHECKSUM,
    ASTEROS_IPV4_INVALID_TTL,
    ASTEROS_IPV4_FRAGMENTED_PACKET,
    ASTEROS_IPV4_NO_HANDLER
} asteros_ipv4_result_t;

typedef void (*asteros_ipv4_protocol_handler_fn)(
    asteros_netif_t *interface, const uint8_t *source_mac,
    const uint8_t *destination_mac, const uint8_t *source_address,
    const uint8_t *destination_address, const uint8_t *payload,
    uint16_t payload_length, void *handler_context);

asteros_ipv4_result_t asteros_ipv4_init(void);
asteros_ipv4_result_t asteros_ipv4_configure(
    asteros_netif_t *interface, const uint8_t address[4]);
asteros_ipv4_result_t asteros_ipv4_register_protocol(
    uint8_t protocol, asteros_ipv4_protocol_handler_fn handler,
    void *handler_context);

/* Accept only complete, unfragmented packets addressed to a configured
   local interface. Dispatch is synchronous and allocation-free. */
asteros_ipv4_result_t asteros_ipv4_receive(
    asteros_netif_t *interface, const uint8_t *source_mac,
    const uint8_t *destination_mac, const uint8_t *packet,
    uint16_t packet_length);

uint16_t asteros_ipv4_checksum(const uint8_t *bytes, uint16_t length);
const char *asteros_ipv4_status_message(asteros_ipv4_result_t result);
#endif

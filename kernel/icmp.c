#include "icmp.h"
#include "ipv4.h"

#define ICMP_PROTOCOL_NUMBER 1u
#define ICMP_ECHO_REQUEST 8u
#define ICMP_ECHO_REPLY 0u
#define ICMP_HEADER_SIZE 8u

static uint8_t initialized;

static void write_be16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}
static void copy_bytes(uint8_t *destination, const uint8_t *source,
                       uint16_t length) {
    uint16_t index;
    for (index = 0; index < length; index++) destination[index] = source[index];
}

static void receive_icmp(
    asteros_netif_t *interface, const uint8_t *source_mac,
    const uint8_t *destination_mac, const uint8_t *source_address,
    const uint8_t *destination_address, const uint8_t *payload,
    uint16_t payload_length, void *context) {
    uint8_t frame[ASTEROS_ETHERNET_HEADER_SIZE + ASTEROS_IPV4_HEADER_MIN +
                  ASTEROS_ETHERNET_MTU_MAX];
    uint8_t *ip, *icmp;
    uint16_t ip_length, frame_length, index;
    (void)destination_mac;
    (void)context;
    if (!interface || !source_mac || !source_address || !destination_address ||
        !payload || payload_length < ICMP_HEADER_SIZE ||
        payload_length > (uint16_t)(interface->mtu - ASTEROS_IPV4_HEADER_MIN))
        return;
    if (payload[0] != ICMP_ECHO_REQUEST || payload[1] != 0u ||
        asteros_ipv4_checksum(payload, payload_length) != 0u)
        return;

    for (index = 0; index < 6u; index++) {
        frame[index] = source_mac[index];
        frame[index + 6u] = interface->mac[index];
    }
    frame[12] = 0x08u; frame[13] = 0x00u;
    ip = frame + ASTEROS_ETHERNET_HEADER_SIZE;
    ip_length = (uint16_t)(ASTEROS_IPV4_HEADER_MIN + payload_length);
    ip[0] = 0x45u; ip[1] = 0u;
    write_be16(ip + 2u, ip_length);
    write_be16(ip + 4u, 0u);
    write_be16(ip + 6u, 0x4000u);
    ip[8] = 64u; ip[9] = ICMP_PROTOCOL_NUMBER;
    ip[10] = 0u; ip[11] = 0u;
    copy_bytes(ip + 12u, destination_address, 4u);
    copy_bytes(ip + 16u, source_address, 4u);
    write_be16(ip + 10u, asteros_ipv4_checksum(ip, ASTEROS_IPV4_HEADER_MIN));
    icmp = ip + ASTEROS_IPV4_HEADER_MIN;
    copy_bytes(icmp, payload, payload_length);
    icmp[0] = ICMP_ECHO_REPLY;
    icmp[2] = 0u; icmp[3] = 0u;
    write_be16(icmp + 2u, asteros_ipv4_checksum(icmp, payload_length));
    frame_length = (uint16_t)(ASTEROS_ETHERNET_HEADER_SIZE + ip_length);
    (void)asteros_netif_send_frame(interface, frame, frame_length);
}

asteros_icmp_result_t asteros_icmp_init(void) {
    if (initialized) return ASTEROS_ICMP_ALREADY_INITIALIZED;
    if (asteros_ipv4_register_protocol(ICMP_PROTOCOL_NUMBER,
                                       receive_icmp, 0) != ASTEROS_IPV4_OK)
        return ASTEROS_ICMP_HANDLER_REGISTRATION_FAILED;
    initialized = 1u;
    return ASTEROS_ICMP_OK;
}

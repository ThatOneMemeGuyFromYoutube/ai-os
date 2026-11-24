#include "ipv4.h"

typedef struct {
    asteros_netif_t *interface;
    uint8_t address[4];
} ipv4_binding_t;

typedef struct {
    uint8_t protocol;
    asteros_ipv4_protocol_handler_fn handler;
    void *context;
} ipv4_protocol_t;

static ipv4_binding_t bindings[ASTEROS_NET_MAX_INTERFACES];
static ipv4_protocol_t protocols[ASTEROS_IPV4_MAX_PROTOCOL_HANDLERS];
static uint8_t initialized;

static uint16_t read_be16(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}
static void write_be16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}
static uint8_t bytes_equal(const uint8_t *left, const uint8_t *right,
                           uint8_t length) {
    uint8_t index;
    for (index = 0; index < length; index++)
        if (left[index] != right[index]) return 0;
    return 1;
}
static uint8_t mac_valid(const uint8_t address[6]) {
    uint8_t index, nonzero = 0;
    if (!address || (address[0] & 1u)) return 0;
    for (index = 0; index < 6u; index++)
        if (address[index]) nonzero = 1;
    return nonzero;
}
static uint8_t ipv4_address_valid(const uint8_t address[4]) {
    return (uint8_t)(address && address[0] != 0u && address[0] < 224u);
}
static ipv4_binding_t *find_binding(asteros_netif_t *interface) {
    uint8_t index;
    for (index = 0; index < ASTEROS_NET_MAX_INTERFACES; index++)
        if (bindings[index].interface == interface) return &bindings[index];
    return 0;
}
static int8_t find_protocol(uint8_t protocol) {
    uint8_t index;
    for (index = 0; index < ASTEROS_IPV4_MAX_PROTOCOL_HANDLERS; index++)
        if (protocols[index].handler && protocols[index].protocol == protocol)
            return (int8_t)index;
    return -1;
}

uint16_t asteros_ipv4_checksum(const uint8_t *bytes, uint16_t length) {
    uint32_t sum = 0;
    uint16_t index = 0;
    if (!bytes && length) return 0;
    while ((uint16_t)(index + 1u) < length) {
        sum += (uint32_t)(((uint16_t)bytes[index] << 8) | bytes[index + 1u]);
        index = (uint16_t)(index + 2u);
    }
    if (index < length) sum += (uint32_t)((uint16_t)bytes[index] << 8);
    while (sum >> 16) sum = (sum & 0xFFFFu) + (sum >> 16);
    return (uint16_t)~sum;
}

static void receive_ethernet_ipv4(
    asteros_netif_t *interface, const uint8_t *source_mac,
    const uint8_t *destination_mac, const uint8_t *packet,
    uint16_t packet_length, void *context) {
    (void)context;
    (void)asteros_ipv4_receive(interface, source_mac, destination_mac,
                               packet, packet_length);
}

asteros_ipv4_result_t asteros_ipv4_init(void) {
    uint8_t index;
    if (initialized) return ASTEROS_IPV4_ALREADY_INITIALIZED;
    for (index = 0; index < ASTEROS_NET_MAX_INTERFACES; index++)
        bindings[index].interface = 0;
    for (index = 0; index < ASTEROS_IPV4_MAX_PROTOCOL_HANDLERS; index++) {
        protocols[index].protocol = 0;
        protocols[index].handler = 0;
        protocols[index].context = 0;
    }
    if (asteros_net_register_ethertype(ASTEROS_IPV4_ETHERTYPE,
                                       receive_ethernet_ipv4, 0) != ASTEROS_NET_OK)
        return ASTEROS_IPV4_HANDLER_REGISTRATION_FAILED;
    initialized = 1u;
    return ASTEROS_IPV4_OK;
}

asteros_ipv4_result_t asteros_ipv4_configure(
    asteros_netif_t *interface, const uint8_t address[4]) {
    ipv4_binding_t *binding;
    uint8_t index;
    if (!interface || !address) return ASTEROS_IPV4_INVALID_ARGUMENT;
    if (!initialized) return ASTEROS_IPV4_NOT_INITIALIZED;
    if (!asteros_netif_is_registered(interface))
        return ASTEROS_IPV4_INTERFACE_NOT_REGISTERED;
    if (!ipv4_address_valid(address)) return ASTEROS_IPV4_INVALID_ADDRESS;
    binding = find_binding(interface);
    for (index = 0; index < ASTEROS_NET_MAX_INTERFACES; index++) {
        if (bindings[index].interface && bindings[index].interface != interface &&
            bytes_equal(bindings[index].address, address, 4u))
            return ASTEROS_IPV4_ADDRESS_IN_USE;
    }
    if (!binding) {
        for (index = 0; index < ASTEROS_NET_MAX_INTERFACES; index++) {
            if (!bindings[index].interface) {
                binding = &bindings[index];
                binding->interface = interface;
                break;
            }
        }
    }
    if (!binding) return ASTEROS_IPV4_TABLE_FULL;
    for (index = 0; index < 4u; index++) binding->address[index] = address[index];
    return ASTEROS_IPV4_OK;
}

asteros_ipv4_result_t asteros_ipv4_register_protocol(
    uint8_t protocol, asteros_ipv4_protocol_handler_fn handler,
    void *handler_context) {
    uint8_t index;
    if (!initialized) return ASTEROS_IPV4_NOT_INITIALIZED;
    if (!handler) return ASTEROS_IPV4_INVALID_ARGUMENT;
    if (find_protocol(protocol) >= 0) return ASTEROS_IPV4_ADDRESS_IN_USE;
    for (index = 0; index < ASTEROS_IPV4_MAX_PROTOCOL_HANDLERS; index++) {
        if (!protocols[index].handler) {
            protocols[index].protocol = protocol;
            protocols[index].handler = handler;
            protocols[index].context = handler_context;
            return ASTEROS_IPV4_OK;
        }
    }
    return ASTEROS_IPV4_TABLE_FULL;
}

asteros_ipv4_result_t asteros_ipv4_receive(
    asteros_netif_t *interface, const uint8_t *source_mac,
    const uint8_t *destination_mac, const uint8_t *packet,
    uint16_t packet_length) {
    ipv4_binding_t *binding;
    ipv4_protocol_t *entry;
    uint16_t header_length, total_length, fragment;
    uint8_t protocol;
    int8_t slot;

    if (!interface || !source_mac || !destination_mac || !packet)
        return ASTEROS_IPV4_INVALID_ARGUMENT;
    if (!initialized) return ASTEROS_IPV4_NOT_INITIALIZED;
    if (!asteros_netif_is_registered(interface))
        return ASTEROS_IPV4_INTERFACE_NOT_REGISTERED;
    binding = find_binding(interface);
    if (!binding) return ASTEROS_IPV4_INTERFACE_NOT_CONFIGURED;
    if (!mac_valid(source_mac) || bytes_equal(source_mac, interface->mac, 6u))
        return ASTEROS_IPV4_INVALID_SOURCE;
    if (!bytes_equal(destination_mac, interface->mac, 6u))
        return ASTEROS_IPV4_WRONG_DESTINATION;
    if (packet_length < ASTEROS_IPV4_HEADER_MIN)
        return ASTEROS_IPV4_PACKET_TOO_SHORT;
    if ((packet[0] >> 4) != 4u) return ASTEROS_IPV4_INVALID_VERSION;
    if ((packet[0] & 0x0Fu) < 5u || (packet[0] & 0x0Fu) > 15u)
        return ASTEROS_IPV4_INVALID_HEADER_LENGTH;
    header_length = (uint16_t)((packet[0] & 0x0Fu) * 4u);
    if (header_length > ASTEROS_IPV4_HEADER_MAX || header_length > packet_length)
        return ASTEROS_IPV4_INVALID_HEADER_LENGTH;
    total_length = read_be16(packet + 2u);
    if (total_length < header_length || total_length > packet_length)
        return ASTEROS_IPV4_INVALID_TOTAL_LENGTH;
    if (!packet[8]) return ASTEROS_IPV4_INVALID_TTL;
    fragment = read_be16(packet + 6u);
    /* Permit DF; reject the reserved flag, MF, and every fragment offset. */
    if (fragment & 0xBFFFu) return ASTEROS_IPV4_FRAGMENTED_PACKET;
    if (asteros_ipv4_checksum(packet, header_length) != 0u)
        return ASTEROS_IPV4_INVALID_CHECKSUM;
    if (!ipv4_address_valid(packet + 12u) ||
        bytes_equal(packet + 12u, binding->address, 4u))
        return ASTEROS_IPV4_INVALID_SOURCE;
    if (!bytes_equal(packet + 16u, binding->address, 4u))
        return ASTEROS_IPV4_WRONG_DESTINATION;
    protocol = packet[9];
    slot = find_protocol(protocol);
    if (slot < 0) return ASTEROS_IPV4_NO_HANDLER;
    entry = &protocols[(uint8_t)slot];
    entry->handler(interface, source_mac, destination_mac,
                   packet + 12u, packet + 16u, packet + header_length,
                   (uint16_t)(total_length - header_length), entry->context);
    return ASTEROS_IPV4_OK;
}

const char *asteros_ipv4_status_message(asteros_ipv4_result_t result) {
    switch (result) {
        case ASTEROS_IPV4_OK: return "OK";
        case ASTEROS_IPV4_INVALID_ARGUMENT: return "Invalid IPv4 request";
        case ASTEROS_IPV4_ALREADY_INITIALIZED: return "IPv4 already initialized";
        case ASTEROS_IPV4_NOT_INITIALIZED: return "IPv4 is not initialized";
        case ASTEROS_IPV4_INTERFACE_NOT_REGISTERED: return "Network interface is not registered";
        case ASTEROS_IPV4_INTERFACE_NOT_CONFIGURED: return "IPv4 address is not configured";
        case ASTEROS_IPV4_INVALID_ADDRESS: return "Invalid local IPv4 address";
        case ASTEROS_IPV4_ADDRESS_IN_USE: return "IPv4 address or protocol is already in use";
        case ASTEROS_IPV4_TABLE_FULL: return "IPv4 table is full";
        case ASTEROS_IPV4_HANDLER_REGISTRATION_FAILED: return "Unable to register IPv4 Ethernet handler";
        case ASTEROS_IPV4_PACKET_TOO_SHORT: return "IPv4 packet is too short";
        case ASTEROS_IPV4_INVALID_SOURCE: return "Invalid IPv4 source";
        case ASTEROS_IPV4_WRONG_DESTINATION: return "IPv4 packet is not addressed to this interface";
        case ASTEROS_IPV4_INVALID_VERSION: return "Unsupported IP version";
        case ASTEROS_IPV4_INVALID_HEADER_LENGTH: return "Invalid IPv4 header length";
        case ASTEROS_IPV4_INVALID_TOTAL_LENGTH: return "Invalid IPv4 total length";
        case ASTEROS_IPV4_INVALID_CHECKSUM: return "Invalid IPv4 header checksum";
        case ASTEROS_IPV4_INVALID_TTL: return "IPv4 packet has expired";
        case ASTEROS_IPV4_FRAGMENTED_PACKET: return "IPv4 fragments are not supported";
        case ASTEROS_IPV4_NO_HANDLER: return "No handler for IPv4 protocol";
        default: return "Unknown IPv4 error";
    }
}

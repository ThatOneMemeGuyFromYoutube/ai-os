#include "arp.h"

typedef struct {
    asteros_netif_t *interface;
    uint8_t ipv4_address[4];
} asteros_arp_binding_t;

static asteros_arp_binding_t bindings[ASTEROS_ARP_MAX_INTERFACES];
static uint8_t initialized;

static uint16_t read_be16(const uint8_t *bytes) {
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static void write_be16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static uint8_t ipv4_equal(const uint8_t *left, const uint8_t *right) {
    uint8_t index;
    for (index = 0; index < 4u; index++) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static uint8_t ipv4_address_valid(const uint8_t address[4]) {
    /* Zero-net and multicast/reserved class-D/E addresses are not local
       unicast addresses. The subnet mask is not known at this layer, so
       directed-broadcast validation belongs to a future IPv4 configuration. */
    return (uint8_t)(address[0] != 0u && address[0] < 224u);
}

static uint8_t mac_address_equal(const uint8_t *left, const uint8_t *right) {
    uint8_t index;
    for (index = 0; index < 6u; index++) {
        if (left[index] != right[index]) return 0;
    }
    return 1;
}

static uint8_t mac_address_valid(const uint8_t address[6]) {
    uint8_t index;
    uint8_t any_nonzero = 0;

    if (address[0] & 1u) return 0;
    for (index = 0; index < 6u; index++) {
        if (address[index]) any_nonzero = 1;
    }
    return any_nonzero;
}

static asteros_arp_binding_t *find_binding(asteros_netif_t *interface) {
    uint8_t index;
    for (index = 0; index < ASTEROS_ARP_MAX_INTERFACES; index++) {
        if (bindings[index].interface == interface) return &bindings[index];
    }
    return 0;
}

static void receive_arp(asteros_netif_t *interface, const uint8_t *packet,
                        uint16_t length, void *context) {
    asteros_arp_binding_t *binding;
    const uint8_t *sender_mac;
    const uint8_t *sender_ipv4;
    const uint8_t *target_ipv4;
    uint8_t reply[ASTEROS_ARP_FRAME_SIZE];
    uint8_t index;

    (void)context;
    if (!interface || !packet || length < ASTEROS_ARP_PACKET_SIZE) return;
    binding = find_binding(interface);
    if (!binding) return;

    if (read_be16(packet) != 1u || read_be16(packet + 2u) != 0x0800u ||
        packet[4] != 6u || packet[5] != 4u ||
        read_be16(packet + 6u) != 1u)
        return;

    sender_mac = packet + 8u;
    sender_ipv4 = packet + 14u;
    target_ipv4 = packet + 24u;
    if (!mac_address_valid(sender_mac) ||
        mac_address_equal(sender_mac, interface->mac) ||
        !ipv4_equal(target_ipv4, binding->ipv4_address))
        return;

    for (index = 0; index < 6u; index++) {
        reply[index] = sender_mac[index];
        reply[index + 6u] = interface->mac[index];
    }
    reply[12] = 0x08u;
    reply[13] = 0x06u;

    write_be16(reply + 14u, 1u);       /* Ethernet hardware. */
    write_be16(reply + 16u, 0x0800u); /* IPv4 protocol. */
    reply[18] = 6u;
    reply[19] = 4u;
    write_be16(reply + 20u, 2u);       /* ARP reply. */
    for (index = 0; index < 6u; index++) {
        reply[22u + index] = interface->mac[index];
        reply[32u + index] = sender_mac[index];
    }
    for (index = 0; index < 4u; index++) {
        reply[28u + index] = binding->ipv4_address[index];
        reply[38u + index] = sender_ipv4[index];
    }

    (void)asteros_netif_send_frame(interface, reply, sizeof reply);
}

asteros_arp_result_t asteros_arp_init(void) {
    uint8_t index;

    if (initialized) return ASTEROS_ARP_ALREADY_INITIALIZED;
    for (index = 0; index < ASTEROS_ARP_MAX_INTERFACES; index++) {
        bindings[index].interface = 0;
        bindings[index].ipv4_address[0] = 0;
        bindings[index].ipv4_address[1] = 0;
        bindings[index].ipv4_address[2] = 0;
        bindings[index].ipv4_address[3] = 0;
    }
    if (asteros_net_register_ethertype(ASTEROS_ARP_ETHERTYPE,
                                       receive_arp, 0) != ASTEROS_NET_OK)
        return ASTEROS_ARP_HANDLER_REGISTRATION_FAILED;
    initialized = 1u;
    return ASTEROS_ARP_OK;
}

asteros_arp_result_t asteros_arp_configure(
    asteros_netif_t *interface, const uint8_t ipv4_address[4]) {
    asteros_arp_binding_t *binding;
    uint8_t index;

    if (!interface || !ipv4_address) return ASTEROS_ARP_INVALID_ARGUMENT;
    if (!initialized) return ASTEROS_ARP_NOT_INITIALIZED;
    if (!asteros_netif_is_registered(interface))
        return ASTEROS_ARP_INTERFACE_NOT_REGISTERED;
    if (!ipv4_address_valid(ipv4_address)) return ASTEROS_ARP_INVALID_ADDRESS;

    binding = find_binding(interface);
    if (!binding) {
        for (index = 0; index < ASTEROS_ARP_MAX_INTERFACES; index++) {
            if (!bindings[index].interface) {
                binding = &bindings[index];
                binding->interface = interface;
                break;
            }
        }
    }
    if (!binding) return ASTEROS_ARP_TABLE_FULL;

    for (index = 0; index < 4u; index++)
        binding->ipv4_address[index] = ipv4_address[index];
    return ASTEROS_ARP_OK;
}

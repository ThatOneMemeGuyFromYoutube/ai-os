#include "net.h"

typedef struct {
    uint16_t ethertype;
    asteros_net_ethertype_handler_fn handler;
    void *context;
} asteros_net_protocol_t;

static asteros_netif_t *interfaces[ASTEROS_NET_MAX_INTERFACES];
static uint8_t interface_count;
static asteros_net_protocol_t protocols[ASTEROS_NET_MAX_PROTOCOL_HANDLERS];

static uint8_t interface_name_valid(const char *name) {
    uint8_t index;

    if (!name || !name[0]) return 0;
    for (index = 0; index < ASTEROS_NETIF_NAME_MAX; index++) {
        if (name[index] == '\0') return 1;
    }
    return 0;
}

static uint8_t interface_names_equal(const char *left, const char *right) {
    uint8_t index;

    for (index = 0; index < ASTEROS_NETIF_NAME_MAX; index++) {
        if (left[index] != right[index]) return 0;
        if (left[index] == '\0') return 1;
    }
    return 1;
}

static uint8_t mac_address_valid(const uint8_t mac[6]) {
    uint8_t index;
    uint8_t any_nonzero = 0;

    if (mac[0] & 1u) return 0; /* Interface addresses must be unicast. */
    for (index = 0; index < 6; index++) {
        if (mac[index]) any_nonzero = 1;
    }
    return any_nonzero;
}

static int8_t protocol_index(uint16_t ethertype) {
    uint8_t index;

    for (index = 0; index < ASTEROS_NET_MAX_PROTOCOL_HANDLERS; index++) {
        if (protocols[index].handler &&
            protocols[index].ethertype == ethertype)
            return (int8_t)index;
    }
    return -1;
}

void asteros_net_init(void) {
    uint8_t index;

    for (index = 0; index < ASTEROS_NET_MAX_INTERFACES; index++)
        interfaces[index] = 0;
    for (index = 0; index < ASTEROS_NET_MAX_PROTOCOL_HANDLERS; index++) {
        protocols[index].ethertype = 0;
        protocols[index].handler = 0;
        protocols[index].context = 0;
    }
    interface_count = 0;
}

uint8_t asteros_netif_is_registered(const asteros_netif_t *interface) {
    uint8_t index;

    if (!interface) return 0;
    for (index = 0; index < interface_count; index++) {
        if (interfaces[index] == interface) return 1;
    }
    return 0;
}

asteros_net_result_t asteros_netif_register(asteros_netif_t *interface) {
    uint8_t index;

    if (!interface || !interface->transmit ||
        !interface_name_valid(interface->name) ||
        !mac_address_valid(interface->mac) ||
        interface->mtu < ASTEROS_ETHERNET_MTU_MIN ||
        interface->mtu > ASTEROS_ETHERNET_MTU_MAX ||
        interface->link_up > 1u)
        return ASTEROS_NET_INVALID_ARGUMENT;

    for (index = 0; index < interface_count; index++) {
        if (interfaces[index] == interface ||
            interface_names_equal(interfaces[index]->name, interface->name))
            return ASTEROS_NET_ALREADY_REGISTERED;
    }
    if (interface_count >= ASTEROS_NET_MAX_INTERFACES)
        return ASTEROS_NET_TABLE_FULL;

    interfaces[interface_count++] = interface;
    return ASTEROS_NET_OK;
}

asteros_net_result_t asteros_netif_set_link(asteros_netif_t *interface,
                                             uint8_t link_up) {
    if (link_up > 1u) return ASTEROS_NET_INVALID_ARGUMENT;
    if (!asteros_netif_is_registered(interface))
        return ASTEROS_NET_NOT_REGISTERED;
    interface->link_up = link_up;
    return ASTEROS_NET_OK;
}

uint8_t asteros_netif_count(void) {
    return interface_count;
}

asteros_netif_t *asteros_netif_at(uint8_t index) {
    if (index >= interface_count) return 0;
    return interfaces[index];
}

asteros_net_result_t asteros_net_register_ethertype(
    uint16_t ethertype, asteros_net_ethertype_handler_fn handler,
    void *handler_context) {
    uint8_t index;

    if (ethertype < ASTEROS_ETHERTYPE_MIN || !handler)
        return ASTEROS_NET_INVALID_ARGUMENT;
    if (protocol_index(ethertype) >= 0)
        return ASTEROS_NET_ALREADY_REGISTERED;

    for (index = 0; index < ASTEROS_NET_MAX_PROTOCOL_HANDLERS; index++) {
        if (!protocols[index].handler) {
            protocols[index].ethertype = ethertype;
            protocols[index].handler = handler;
            protocols[index].context = handler_context;
            return ASTEROS_NET_OK;
        }
    }
    return ASTEROS_NET_TABLE_FULL;
}

asteros_net_result_t asteros_netif_send_frame(asteros_netif_t *interface,
                                               const uint8_t *frame,
                                               uint16_t length) {
    if (!interface || !frame) return ASTEROS_NET_INVALID_ARGUMENT;
    if (!asteros_netif_is_registered(interface))
        return ASTEROS_NET_NOT_REGISTERED;
    if (!interface->link_up) return ASTEROS_NET_LINK_DOWN;
    if (length < ASTEROS_ETHERNET_HEADER_SIZE)
        return ASTEROS_NET_FRAME_TOO_SHORT;
    if (length > (uint16_t)(interface->mtu + ASTEROS_ETHERNET_HEADER_SIZE))
        return ASTEROS_NET_FRAME_TOO_LARGE;
    return interface->transmit(interface->driver_context, frame, length)
        ? ASTEROS_NET_OK : ASTEROS_NET_TRANSMIT_FAILED;
}

asteros_net_result_t asteros_netif_receive_frame(asteros_netif_t *interface,
                                                  const uint8_t *frame,
                                                  uint16_t length) {
    uint16_t ethertype;
    int8_t index;
    asteros_net_protocol_t *protocol;

    if (!interface || !frame) return ASTEROS_NET_INVALID_ARGUMENT;
    if (!asteros_netif_is_registered(interface))
        return ASTEROS_NET_NOT_REGISTERED;
    if (!interface->link_up) return ASTEROS_NET_LINK_DOWN;
    if (length < ASTEROS_ETHERNET_HEADER_SIZE)
        return ASTEROS_NET_FRAME_TOO_SHORT;
    if (length > (uint16_t)(interface->mtu + ASTEROS_ETHERNET_HEADER_SIZE))
        return ASTEROS_NET_FRAME_TOO_LARGE;

    ethertype = (uint16_t)(((uint16_t)frame[12] << 8) | frame[13]);
    /* Values below 0x0600 are IEEE 802.3 payload lengths, not EtherTypes.
       LLC/SNAP framing is intentionally outside this first core revision. */
    if (ethertype < ASTEROS_ETHERTYPE_MIN)
        return ASTEROS_NET_NOT_ETHERNET_II;

    index = protocol_index(ethertype);
    if (index < 0) return ASTEROS_NET_NO_HANDLER;

    protocol = &protocols[(uint8_t)index];
    protocol->handler(interface, frame + 6u, frame,
                      frame + ASTEROS_ETHERNET_HEADER_SIZE,
                      (uint16_t)(length - ASTEROS_ETHERNET_HEADER_SIZE),
                      protocol->context);
    return ASTEROS_NET_OK;
}

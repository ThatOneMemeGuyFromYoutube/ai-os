#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "net.h"

#define TEST_FRAME_SIZE (ASTEROS_ETHERNET_HEADER_SIZE + 4u)

typedef struct {
    uint8_t transmit_result;
    uint8_t transmit_calls;
    uint16_t last_length;
    uint8_t last_frame[ASTEROS_ETHERNET_MTU_MAX + ASTEROS_ETHERNET_HEADER_SIZE];
} mock_driver_t;

typedef struct {
    uint8_t calls;
    asteros_netif_t *interface;
    uint8_t source_mac[6];
    uint8_t destination_mac[6];
    uint16_t payload_length;
    uint8_t payload[8];
} mock_protocol_t;

static uint8_t mock_transmit(void *context, const uint8_t *frame,
                             uint16_t length) {
    mock_driver_t *driver = (mock_driver_t *)context;
    driver->transmit_calls++;
    driver->last_length = length;
    if (length <= sizeof driver->last_frame)
        memcpy(driver->last_frame, frame, length);
    return driver->transmit_result;
}

static void mock_ipv4_handler(asteros_netif_t *interface,
                              const uint8_t *source_mac,
                              const uint8_t *destination_mac,
                              const uint8_t *payload, uint16_t length,
                              void *context) {
    mock_protocol_t *protocol = (mock_protocol_t *)context;
    protocol->calls++;
    protocol->interface = interface;
    memcpy(protocol->source_mac, source_mac, 6u);
    memcpy(protocol->destination_mac, destination_mac, 6u);
    protocol->payload_length = length;
    if (length <= sizeof protocol->payload)
        memcpy(protocol->payload, payload, length);
}

static void prepare_interface(asteros_netif_t *interface, const char *name,
                              uint8_t mac_suffix, mock_driver_t *driver,
                              uint16_t mtu, uint8_t link_up) {
    size_t index;

    memset(interface, 0, sizeof *interface);
    for (index = 0; name[index] && index + 1u < sizeof interface->name; index++)
        interface->name[index] = name[index];
    interface->mac[0] = 0x02;
    interface->mac[1] = 0xA5;
    interface->mac[2] = 0x7E;
    interface->mac[3] = 0x10;
    interface->mac[4] = 0x20;
    interface->mac[5] = mac_suffix;
    interface->mtu = mtu;
    interface->link_up = link_up;
    interface->driver_context = driver;
    interface->transmit = mock_transmit;
}

static void make_ipv4_frame(uint8_t frame[TEST_FRAME_SIZE]) {
    memset(frame, 0, TEST_FRAME_SIZE);
    memset(frame, 0xFF, 6u); /* Broadcast destination is valid for receive tests. */
    frame[6] = 0x02;
    frame[11] = 0x44;
    frame[12] = 0x08;
    frame[13] = 0x00;
    frame[14] = 0x45;
    frame[15] = 0x00;
    frame[16] = 0x00;
    frame[17] = 0x04;
}

static void test_interface_registration_and_enumeration(void) {
    asteros_netif_t first;
    asteros_netif_t duplicate;
    mock_driver_t driver = {1, 0, 0, {0}};

    asteros_net_init();
    assert(asteros_netif_count() == 0);
    assert(asteros_netif_at(0) == 0);
    assert(!asteros_netif_is_registered(0));

    prepare_interface(&first, "eth0", 1, &driver, 1500, 0);
    assert(asteros_netif_register(&first) == ASTEROS_NET_OK);
    assert(asteros_netif_count() == 1);
    assert(asteros_netif_at(0) == &first);
    assert(asteros_netif_at(1) == 0);
    assert(asteros_netif_is_registered(&first));
    assert(asteros_netif_register(&first) == ASTEROS_NET_ALREADY_REGISTERED);

    prepare_interface(&duplicate, "eth0", 2, &driver, 1500, 1);
    assert(asteros_netif_register(&duplicate) == ASTEROS_NET_ALREADY_REGISTERED);
    assert(asteros_netif_count() == 1);
}

static void test_registration_rejects_invalid_interfaces(void) {
    asteros_netif_t interface;
    mock_driver_t driver = {1, 0, 0, {0}};

    asteros_net_init();
    prepare_interface(&interface, "eth0", 1, &driver, 1500, 0);
    interface.name[0] = '\0';
    assert(asteros_netif_register(&interface) == ASTEROS_NET_INVALID_ARGUMENT);

    prepare_interface(&interface, "eth0", 1, &driver, 1500, 0);
    memset(interface.name, 'x', sizeof interface.name);
    assert(asteros_netif_register(&interface) == ASTEROS_NET_INVALID_ARGUMENT);

    prepare_interface(&interface, "eth0", 1, &driver, 67, 0);
    assert(asteros_netif_register(&interface) == ASTEROS_NET_INVALID_ARGUMENT);
    prepare_interface(&interface, "eth0", 1, &driver, 1501, 0);
    assert(asteros_netif_register(&interface) == ASTEROS_NET_INVALID_ARGUMENT);

    prepare_interface(&interface, "eth0", 1, &driver, 1500, 2);
    assert(asteros_netif_register(&interface) == ASTEROS_NET_INVALID_ARGUMENT);

    prepare_interface(&interface, "eth0", 1, &driver, 1500, 0);
    interface.mac[0] = 0x01; /* Multicast address. */
    assert(asteros_netif_register(&interface) == ASTEROS_NET_INVALID_ARGUMENT);
    prepare_interface(&interface, "eth0", 1, &driver, 1500, 0);
    memset(interface.mac, 0, sizeof interface.mac);
    assert(asteros_netif_register(&interface) == ASTEROS_NET_INVALID_ARGUMENT);
    prepare_interface(&interface, "eth0", 1, &driver, 1500, 0);
    interface.transmit = 0;
    assert(asteros_netif_register(&interface) == ASTEROS_NET_INVALID_ARGUMENT);
    assert(asteros_netif_register(0) == ASTEROS_NET_INVALID_ARGUMENT);
    assert(asteros_netif_count() == 0);

    /* The Ethernet minimum MTU is accepted at the exact lower boundary. */
    asteros_net_init();
    prepare_interface(&interface, "eth0", 1, &driver, ASTEROS_ETHERNET_MTU_MIN, 0);
    assert(asteros_netif_register(&interface) == ASTEROS_NET_OK);
}

static void test_interface_registry_is_bounded(void) {
    asteros_netif_t interfaces[ASTEROS_NET_MAX_INTERFACES + 1u];
    mock_driver_t drivers[ASTEROS_NET_MAX_INTERFACES + 1u] = {{0} };
    uint8_t index;

    asteros_net_init();
    for (index = 0; index < ASTEROS_NET_MAX_INTERFACES; index++) {
        char name[] = "eth0";
        name[3] = (char)('0' + index);
        prepare_interface(&interfaces[index], name, (uint8_t)(index + 1u),
                          &drivers[index], 1500, 1);
        assert(asteros_netif_register(&interfaces[index]) == ASTEROS_NET_OK);
    }
    prepare_interface(&interfaces[ASTEROS_NET_MAX_INTERFACES], "eth4", 5,
                      &drivers[ASTEROS_NET_MAX_INTERFACES], 1500, 1);
    assert(asteros_netif_register(
        &interfaces[ASTEROS_NET_MAX_INTERFACES]) == ASTEROS_NET_TABLE_FULL);
    assert(asteros_netif_count() == ASTEROS_NET_MAX_INTERFACES);
}

static void test_link_state_and_frame_transmit(void) {
    asteros_netif_t interface;
    asteros_netif_t unregistered;
    mock_driver_t driver = {1, 0, 0, {0}};
    uint8_t frame[TEST_FRAME_SIZE];
    uint8_t short_frame[ASTEROS_ETHERNET_HEADER_SIZE - 1u];
    uint8_t max_frame[ASTEROS_ETHERNET_MTU_MAX +
                      ASTEROS_ETHERNET_HEADER_SIZE];
    uint8_t oversized[ASTEROS_ETHERNET_MTU_MAX +
                      ASTEROS_ETHERNET_HEADER_SIZE + 1u];

    asteros_net_init();
    prepare_interface(&interface, "eth0", 1, &driver, 1500, 0);
    prepare_interface(&unregistered, "eth1", 2, &driver, 1500, 1);
    assert(asteros_netif_register(&interface) == ASTEROS_NET_OK);
    make_ipv4_frame(frame);

    assert(asteros_netif_send_frame(&unregistered, frame, sizeof frame) ==
           ASTEROS_NET_NOT_REGISTERED);
    assert(asteros_netif_send_frame(&interface, frame, sizeof frame) ==
           ASTEROS_NET_LINK_DOWN);
    assert(asteros_netif_set_link(&interface, 2) == ASTEROS_NET_INVALID_ARGUMENT);
    assert(asteros_netif_set_link(&unregistered, 1) == ASTEROS_NET_NOT_REGISTERED);
    assert(asteros_netif_set_link(&interface, 1) == ASTEROS_NET_OK);
    assert(asteros_netif_set_link(&interface, 0) == ASTEROS_NET_OK);
    assert(asteros_netif_set_link(&interface, 1) == ASTEROS_NET_OK);

    assert(asteros_netif_send_frame(&interface, 0, sizeof frame) ==
           ASTEROS_NET_INVALID_ARGUMENT);
    assert(asteros_netif_send_frame(&interface, frame, sizeof short_frame) ==
           ASTEROS_NET_FRAME_TOO_SHORT);
    assert(asteros_netif_send_frame(&interface, oversized, sizeof oversized) ==
           ASTEROS_NET_FRAME_TOO_LARGE);

    memset(max_frame, 0xA5, sizeof max_frame);
    assert(asteros_netif_send_frame(&interface, max_frame, sizeof max_frame) ==
           ASTEROS_NET_OK);
    assert(driver.transmit_calls == 1);
    assert(driver.last_length == sizeof max_frame);
    assert(memcmp(driver.last_frame, max_frame, sizeof max_frame) == 0);

    assert(asteros_netif_send_frame(&interface, frame, sizeof frame) ==
           ASTEROS_NET_OK);
    assert(driver.transmit_calls == 2);
    assert(driver.last_length == sizeof frame);
    assert(memcmp(driver.last_frame, frame, sizeof frame) == 0);

    driver.transmit_result = 0;
    assert(asteros_netif_send_frame(&interface, frame, sizeof frame) ==
           ASTEROS_NET_TRANSMIT_FAILED);
    assert(driver.transmit_calls == 3);
}

static void test_receive_validates_and_dispatches_ethernet_ii(void) {
    asteros_netif_t interface;
    mock_driver_t driver = {1, 0, 0, {0}};
    mock_protocol_t protocol = {0, 0, 0, {0}};
    uint8_t frame[TEST_FRAME_SIZE];
    uint8_t short_frame[ASTEROS_ETHERNET_HEADER_SIZE - 1u];
    uint8_t oversized[ASTEROS_ETHERNET_MTU_MAX +
                      ASTEROS_ETHERNET_HEADER_SIZE + 1u];

    asteros_net_init();
    prepare_interface(&interface, "eth0", 1, &driver, 1500, 1);
    assert(asteros_netif_register(&interface) == ASTEROS_NET_OK);
    make_ipv4_frame(frame);

    assert(asteros_net_register_ethertype(0x0800, mock_ipv4_handler,
                                           &protocol) == ASTEROS_NET_OK);
    assert(asteros_net_register_ethertype(0x0800, mock_ipv4_handler,
                                           &protocol) ==
           ASTEROS_NET_ALREADY_REGISTERED);
    assert(asteros_net_register_ethertype(0x05FF, mock_ipv4_handler,
                                           &protocol) ==
           ASTEROS_NET_INVALID_ARGUMENT);
    assert(asteros_net_register_ethertype(0x0806, 0, &protocol) ==
           ASTEROS_NET_INVALID_ARGUMENT);

    assert(asteros_netif_receive_frame(&interface, 0, sizeof frame) ==
           ASTEROS_NET_INVALID_ARGUMENT);
    assert(asteros_netif_receive_frame(&interface, short_frame,
                                       sizeof short_frame) ==
           ASTEROS_NET_FRAME_TOO_SHORT);
    assert(asteros_netif_receive_frame(&interface, oversized,
                                       sizeof oversized) ==
           ASTEROS_NET_FRAME_TOO_LARGE);

    frame[12] = 0x05;
    frame[13] = 0xDC; /* 1500 is an 802.3 length, not an EtherType. */
    assert(asteros_netif_receive_frame(&interface, frame, sizeof frame) ==
           ASTEROS_NET_NOT_ETHERNET_II);

    make_ipv4_frame(frame);
    frame[12] = 0x08;
    frame[13] = 0x06; /* ARP has no registered handler yet. */
    assert(asteros_netif_receive_frame(&interface, frame, sizeof frame) ==
           ASTEROS_NET_NO_HANDLER);
    frame[13] = 0x00;
    assert(asteros_netif_receive_frame(&interface, frame, sizeof frame) ==
           ASTEROS_NET_OK);
    assert(protocol.calls == 1);
    assert(protocol.interface == &interface);
    assert(memcmp(protocol.source_mac, frame + 6u, 6u) == 0);
    assert(memcmp(protocol.destination_mac, frame, 6u) == 0);
    assert(protocol.payload_length == 4);
    assert(memcmp(protocol.payload, frame + ASTEROS_ETHERNET_HEADER_SIZE, 4) == 0);

    assert(asteros_netif_set_link(&interface, 0) == ASTEROS_NET_OK);
    assert(asteros_netif_receive_frame(&interface, frame, sizeof frame) ==
           ASTEROS_NET_LINK_DOWN);
    assert(protocol.calls == 1);
}

static void test_protocol_handler_registry_is_bounded(void) {
    uint16_t index;
    asteros_net_init();

    for (index = 0; index < ASTEROS_NET_MAX_PROTOCOL_HANDLERS; index++) {
        uint16_t type = (uint16_t)(0x88B5u + index);
        assert(asteros_net_register_ethertype(type, mock_ipv4_handler, 0) ==
               ASTEROS_NET_OK);
    }
    assert(asteros_net_register_ethertype(0x88C0, mock_ipv4_handler, 0) ==
           ASTEROS_NET_TABLE_FULL);
}

int main(void) {
    test_interface_registration_and_enumeration();
    test_registration_rejects_invalid_interfaces();
    test_interface_registry_is_bounded();
    test_link_state_and_frame_transmit();
    test_receive_validates_and_dispatches_ethernet_ii();
    test_protocol_handler_registry_is_bounded();
    puts("network core: tests passed");
    return 0;
}

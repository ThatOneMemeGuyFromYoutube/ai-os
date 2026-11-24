#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "arp.h"

typedef struct {
    uint8_t frame[ASTEROS_ETHERNET_HEADER_SIZE + ASTEROS_ETHERNET_MTU_MAX];
    uint16_t length;
    uint8_t calls;
    uint8_t transmit_success;
} capture_t;

static const uint8_t local_mac[6] = {0x52, 0x54, 0x00, 0x12, 0x34, 0x56};
static const uint8_t peer_mac[6] = {0x52, 0x54, 0x00, 0xAA, 0xBB, 0xCC};
static const uint8_t local_ip[4] = {192, 0, 2, 2};
static const uint8_t peer_ip[4] = {192, 0, 2, 1};

static uint8_t capture_transmit(void *context, const uint8_t *frame,
                                uint16_t length) {
    capture_t *capture = (capture_t *)context;
    assert(length <= sizeof capture->frame);
    memcpy(capture->frame, frame, length);
    capture->length = length;
    capture->calls++;
    return capture->transmit_success;
}

static void prepare_interface(asteros_netif_t *interface,
                              capture_t *capture, const char *name,
                              const uint8_t mac[6]) {
    memset(interface, 0, sizeof *interface);
    strncpy(interface->name, name, sizeof interface->name - 1u);
    memcpy(interface->mac, mac, 6u);
    interface->mtu = ASTEROS_ETHERNET_MTU_MAX;
    interface->link_up = 1u;
    interface->driver_context = capture;
    interface->transmit = capture_transmit;
}

static void write_be16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value >> 8);
    bytes[1] = (uint8_t)value;
}

static void make_request(uint8_t frame[60], const uint8_t sender_mac[6],
                         const uint8_t sender_ip[4],
                         const uint8_t target_ip[4]) {
    uint8_t index;
    memset(frame, 0, 60u);
    memset(frame, 0xFF, 6u);
    memcpy(frame + 6u, sender_mac, 6u);
    frame[12] = 0x08u;
    frame[13] = 0x06u;
    write_be16(frame + 14u, 1u);
    write_be16(frame + 16u, 0x0800u);
    frame[18] = 6u;
    frame[19] = 4u;
    write_be16(frame + 20u, 1u);
    for (index = 0; index < 6u; index++) frame[22u + index] = sender_mac[index];
    for (index = 0; index < 4u; index++) {
        frame[28u + index] = sender_ip[index];
        frame[38u + index] = target_ip[index];
    }
}

static void expect_no_reply(asteros_netif_t *interface, capture_t *capture,
                            const uint8_t *frame, uint16_t length) {
    uint8_t previous_calls = capture->calls;
    assert(asteros_netif_receive_frame(interface, frame, length) ==
           ASTEROS_NET_OK);
    assert(capture->calls == previous_calls);
}

static void test_configuration_and_reply(void) {
    asteros_netif_t interface;
    asteros_netif_t other;
    capture_t capture;
    capture_t other_capture;
    uint8_t frame[60];
    uint8_t invalid_ip[4] = {0, 0, 0, 0};
    uint8_t multicast_ip[4] = {239, 1, 2, 3};
    uint8_t broadcast_ip[4] = {255, 255, 255, 255};
    uint8_t bad_mac[6] = {0, 0, 0, 0, 0, 0};
    uint8_t multicast_mac[6] = {0x01, 0x00, 0x5E, 0, 0, 1};
    uint8_t alternate_ip[4] = {192, 0, 2, 22};
    uint8_t index;

    memset(&capture, 0, sizeof capture);
    memset(&other_capture, 0, sizeof other_capture);
    capture.transmit_success = 1u;
    other_capture.transmit_success = 1u;
    prepare_interface(&interface, &capture, "eth0", local_mac);
    prepare_interface(&other, &other_capture, "eth1", peer_mac);

    asteros_net_init();
    assert(asteros_arp_configure(&interface, local_ip) ==
           ASTEROS_ARP_NOT_INITIALIZED);
    assert(asteros_arp_init() == ASTEROS_ARP_OK);
    assert(asteros_arp_init() == ASTEROS_ARP_ALREADY_INITIALIZED);
    assert(asteros_arp_configure(0, local_ip) == ASTEROS_ARP_INVALID_ARGUMENT);
    assert(asteros_arp_configure(&interface, 0) == ASTEROS_ARP_INVALID_ARGUMENT);
    assert(asteros_arp_configure(&interface, local_ip) ==
           ASTEROS_ARP_INTERFACE_NOT_REGISTERED);
    assert(asteros_netif_register(&interface) == ASTEROS_NET_OK);
    assert(asteros_netif_register(&other) == ASTEROS_NET_OK);
    assert(asteros_arp_configure(&interface, invalid_ip) ==
           ASTEROS_ARP_INVALID_ADDRESS);
    assert(asteros_arp_configure(&interface, multicast_ip) ==
           ASTEROS_ARP_INVALID_ADDRESS);
    assert(asteros_arp_configure(&interface, broadcast_ip) ==
           ASTEROS_ARP_INVALID_ADDRESS);
    assert(asteros_arp_configure(&interface, local_ip) == ASTEROS_ARP_OK);
    assert(asteros_arp_configure(&interface, local_ip) == ASTEROS_ARP_OK);

    make_request(frame, peer_mac, peer_ip, local_ip);
    frame[6] ^= 0x02u; /* Ethernet source must match the ARP sender hardware address. */
    expect_no_reply(&interface, &capture, frame, sizeof frame);

    make_request(frame, peer_mac, peer_ip, local_ip);
    assert(asteros_netif_receive_frame(&interface, frame, sizeof frame) ==
           ASTEROS_NET_OK);
    assert(capture.calls == 1u);
    assert(capture.length == ASTEROS_ARP_FRAME_SIZE);
    assert(memcmp(capture.frame, peer_mac, 6u) == 0);
    assert(memcmp(capture.frame + 6u, local_mac, 6u) == 0);
    assert(capture.frame[12] == 0x08u && capture.frame[13] == 0x06u);
    assert(capture.frame[14] == 0u && capture.frame[15] == 1u);
    assert(capture.frame[16] == 0x08u && capture.frame[17] == 0x00u);
    assert(capture.frame[18] == 6u && capture.frame[19] == 4u);
    assert(capture.frame[20] == 0u && capture.frame[21] == 2u);
    assert(memcmp(capture.frame + 22u, local_mac, 6u) == 0);
    assert(memcmp(capture.frame + 28u, local_ip, 4u) == 0);
    assert(memcmp(capture.frame + 32u, peer_mac, 6u) == 0);
    assert(memcmp(capture.frame + 38u, peer_ip, 4u) == 0);

    /* Only a request for this interface's configured address is answered. */
    make_request(frame, peer_mac, peer_ip, invalid_ip);
    expect_no_reply(&interface, &capture, frame, sizeof frame);
    make_request(frame, peer_mac, peer_ip, local_ip);
    frame[20] = 0u; frame[21] = 2u; /* An ARP reply is not a request. */
    expect_no_reply(&interface, &capture, frame, sizeof frame);
    make_request(frame, peer_mac, peer_ip, local_ip);
    frame[14] = 0u; frame[15] = 2u; /* Ethernet hardware type mismatch. */
    expect_no_reply(&interface, &capture, frame, sizeof frame);
    make_request(frame, peer_mac, peer_ip, local_ip);
    frame[16] = 0x86u; frame[17] = 0xDDu; /* IPv6 protocol. */
    expect_no_reply(&interface, &capture, frame, sizeof frame);
    make_request(frame, peer_mac, peer_ip, local_ip);
    frame[18] = 7u;
    expect_no_reply(&interface, &capture, frame, sizeof frame);
    make_request(frame, peer_mac, peer_ip, local_ip);
    frame[19] = 16u;
    expect_no_reply(&interface, &capture, frame, sizeof frame);
    make_request(frame, peer_mac, peer_ip, local_ip);
    memcpy(frame + 22u, bad_mac, 6u);
    expect_no_reply(&interface, &capture, frame, sizeof frame);
    make_request(frame, peer_mac, peer_ip, local_ip);
    memcpy(frame + 22u, multicast_mac, 6u);
    expect_no_reply(&interface, &capture, frame, sizeof frame);
    make_request(frame, local_mac, peer_ip, local_ip);
    expect_no_reply(&interface, &capture, frame, sizeof frame);
    make_request(frame, peer_mac, peer_ip, local_ip);
    expect_no_reply(&interface, &capture, frame,
                    ASTEROS_ETHERNET_HEADER_SIZE + ASTEROS_ARP_PACKET_SIZE - 1u);
    make_request(frame, peer_mac, peer_ip, local_ip);
    assert(asteros_netif_receive_frame(&other, frame, sizeof frame) ==
           ASTEROS_NET_OK);
    assert(capture.calls == 1u && other_capture.calls == 0u);

    /* Updating an interface address replaces its prior local target. */
    assert(asteros_arp_configure(&interface, alternate_ip) == ASTEROS_ARP_OK);
    make_request(frame, peer_mac, peer_ip, local_ip);
    expect_no_reply(&interface, &capture, frame, sizeof frame);
    make_request(frame, peer_mac, peer_ip, alternate_ip);
    assert(asteros_netif_receive_frame(&interface, frame, sizeof frame) ==
           ASTEROS_NET_OK);
    assert(capture.calls == 2u);
    assert(memcmp(capture.frame + 28u, alternate_ip, 4u) == 0);

    /* The interface registry remains bounded by the network core. */
    for (index = 0; index < ASTEROS_NET_MAX_INTERFACES - 2u; index++) {
        static asteros_netif_t extras[ASTEROS_NET_MAX_INTERFACES - 2u];
        static capture_t extra_captures[ASTEROS_NET_MAX_INTERFACES - 2u];
        uint8_t mac[6] = {0x52, 0x54, 0, 0, 0, 0};
        mac[4] = 0x10u;
        mac[5] = index;
        prepare_interface(&extras[index], &extra_captures[index], "extra", mac);
        extras[index].name[5] = (char)('0' + index);
        assert(asteros_netif_register(&extras[index]) == ASTEROS_NET_OK);
        alternate_ip[3] = (uint8_t)(30u + index);
        assert(asteros_arp_configure(&extras[index], alternate_ip) == ASTEROS_ARP_OK);
    }
    assert(asteros_arp_configure(&interface, peer_ip) == ASTEROS_ARP_OK);
}

int main(void) {
    test_configuration_and_reply();
    puts("ARP responder: tests passed");
    return 0;
}

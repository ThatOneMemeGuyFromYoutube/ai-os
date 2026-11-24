#ifndef ASTEROS_ARP_H
#define ASTEROS_ARP_H

#include <stdint.h>
#include "net.h"

#define ASTEROS_ARP_ETHERTYPE 0x0806u
#define ASTEROS_ARP_PACKET_SIZE 28u
#define ASTEROS_ARP_FRAME_SIZE (ASTEROS_ETHERNET_HEADER_SIZE + ASTEROS_ARP_PACKET_SIZE)
#define ASTEROS_ARP_MAX_INTERFACES ASTEROS_NET_MAX_INTERFACES

typedef enum {
    ASTEROS_ARP_OK = 0,
    ASTEROS_ARP_ALREADY_INITIALIZED,
    ASTEROS_ARP_INVALID_ARGUMENT,
    ASTEROS_ARP_NOT_INITIALIZED,
    ASTEROS_ARP_INTERFACE_NOT_REGISTERED,
    ASTEROS_ARP_INVALID_ADDRESS,
    ASTEROS_ARP_TABLE_FULL,
    ASTEROS_ARP_HANDLER_REGISTRATION_FAILED
} asteros_arp_result_t;

/* Register the bounded Ethernet/IPv4 ARP request handler. Call after
   asteros_net_init() and before configuring an interface. */
asteros_arp_result_t asteros_arp_init(void);

/* Assign the IPv4 address that this interface should answer ARP requests for.
   Reconfiguring an existing interface replaces its address in place. */
asteros_arp_result_t asteros_arp_configure(
    asteros_netif_t *interface, const uint8_t ipv4_address[4]);

#endif

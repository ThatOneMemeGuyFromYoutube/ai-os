#ifndef ASTEROS_NET_H
#define ASTEROS_NET_H

#include <stdint.h>

/* The first network core is Ethernet II only. Names include their trailing
   NUL byte, so an interface name may contain at most seven characters. */
#define ASTEROS_NETIF_NAME_MAX 8u
#define ASTEROS_NET_MAX_INTERFACES 4u
#define ASTEROS_NET_MAX_PROTOCOL_HANDLERS 8u
#define ASTEROS_ETHERNET_HEADER_SIZE 14u
#define ASTEROS_ETHERNET_MTU_MIN 68u
#define ASTEROS_ETHERNET_MTU_MAX 1500u
#define ASTEROS_ETHERTYPE_MIN 0x0600u

typedef enum {
    ASTEROS_NET_OK = 0,
    ASTEROS_NET_INVALID_ARGUMENT,
    ASTEROS_NET_ALREADY_REGISTERED,
    ASTEROS_NET_TABLE_FULL,
    ASTEROS_NET_NOT_REGISTERED,
    ASTEROS_NET_LINK_DOWN,
    ASTEROS_NET_FRAME_TOO_SHORT,
    ASTEROS_NET_FRAME_TOO_LARGE,
    ASTEROS_NET_NOT_ETHERNET_II,
    ASTEROS_NET_NO_HANDLER,
    ASTEROS_NET_TRANSMIT_FAILED
} asteros_net_result_t;

struct asteros_netif;

typedef uint8_t (*asteros_net_transmit_fn)(void *driver_context,
                                            const uint8_t *frame,
                                            uint16_t length);
typedef void (*asteros_net_ethertype_handler_fn)(
    struct asteros_netif *interface, const uint8_t *source_mac,
    const uint8_t *destination_mac, const uint8_t *payload,
    uint16_t payload_length, void *handler_context);

typedef struct asteros_netif {
    char name[ASTEROS_NETIF_NAME_MAX];
    uint8_t mac[6];
    uint16_t mtu;
    uint8_t link_up;
    void *driver_context;
    asteros_net_transmit_fn transmit;
} asteros_netif_t;

/* Reset the fixed-size registries. Call once during kernel initialization;
   drivers must register their interfaces after device discovery. */
void asteros_net_init(void);

asteros_net_result_t asteros_netif_register(asteros_netif_t *interface);
asteros_net_result_t asteros_netif_set_link(asteros_netif_t *interface,
                                             uint8_t link_up);
uint8_t asteros_netif_is_registered(const asteros_netif_t *interface);
uint8_t asteros_netif_count(void);
asteros_netif_t *asteros_netif_at(uint8_t index);

/* Drivers transmit and deliver complete Ethernet II frames. Buffers remain
   owned by the caller and are consumed synchronously; the core never queues
   or retains them. */
asteros_net_result_t asteros_netif_send_frame(asteros_netif_t *interface,
                                               const uint8_t *frame,
                                               uint16_t length);
asteros_net_result_t asteros_netif_receive_frame(asteros_netif_t *interface,
                                                  const uint8_t *frame,
                                                  uint16_t length);

/* Protocol handlers receive only the bytes after the 14-byte Ethernet header.
   Handlers run synchronously and must not retain the payload pointer. */
asteros_net_result_t asteros_net_register_ethertype(
    uint16_t ethertype, asteros_net_ethertype_handler_fn handler,
    void *handler_context);

#endif

# v86 network-device preparation

The browser demo can expose either an emulated NE2000-compatible (NE2K) or VirtIO NIC to the AsterOS guest. The selector changes the hardware presented on the next emulator reset. It does not configure a host relay, attach a network backend, or make the guest network-capable by itself. The demo status deliberately says that the network backend is not configured.

## Guest-side foundation

The kernel now builds and initializes a small hardware-independent Ethernet II core in `kernel/net.c`:

- Drivers register a caller-owned `asteros_netif_t` with a short name, unicast MAC address, MTU, link state, driver context, and synchronous transmit callback.
- The registry is fixed at four interfaces and rejects duplicate names, malformed addresses, unsupported MTUs, and missing transmit callbacks. There is no heap allocation.
- Drivers submit complete Ethernet II frames to `asteros_netif_receive_frame()`. The core checks the 14-byte header and MTU boundary, rejects IEEE 802.3 length fields for now, and dispatches the payload by EtherType to a registered protocol handler.
- Upper layers submit complete frames through `asteros_netif_send_frame()`; the core checks registration, link state, and frame bounds before calling the driver.
- Packet buffers remain owned by the caller and are consumed synchronously. This first revision has no packet queue, interrupt synchronization, VLAN handling, LLC/SNAP parsing, ARP, IPv4, DHCP, UDP, or TCP.

The registry is initialized during `kmain()`, but no interface is registered yet: the current boot path has no PCI enumerator and no NIC driver. Host regression tests use a mock adapter to exercise registration, link transitions, transmit failure, Ethernet II validation, MTU limits, and protocol dispatch without requiring emulator hardware.

## Driver and backend sequence

1. Add PCI configuration-space discovery and identify the guest-visible device/vendor IDs before attempting device initialization.
2. Implement one driver against the network-interface callback contract. NE2K is a reasonable first target for a simple port-I/O driver; VirtIO requires its PCI transport and queue setup and should follow as a separate driver.
3. Feed received frames into the generic Ethernet II dispatcher and verify transmit/receive with a deterministic emulator-side peer.
4. Add protocol layers incrementally (ARP before IPv4, then ICMP/UDP and DHCP as needed) with host-testable packet parsers.
5. Configure a v86 network backend/relay separately from the NIC selector, then add an end-to-end guest connectivity test.

Do not report networking as working until a guest driver, protocol path, and backend have all been exercised together. The existing v86 selector is hardware preparation only.

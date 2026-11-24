# v86 network-device preparation

The browser demo can expose either an emulated NE2000-compatible (NE2K) or VirtIO NIC to the AsterOS guest. The selector changes the hardware presented on the next emulator reset but does not configure a host relay or attach a network backend. The guest now has a static development address (`192.0.2.2`), bounded ARP and IPv4 receive paths, and ICMP echo replies; a deterministic QEMU socket peer verifies a direct ARP + ICMP exchange. The browser demo still has no configured v86 network backend, so this does not provide host or Internet connectivity.

## Guest-side PCI discovery

The kernel now performs a read-only PCI configuration mechanism #1 scan during startup:

- Configuration address/data I/O uses ports `0xCF8` and `0xCFC`. The scan runs synchronously while maskable interrupts remain disabled.
- Enumeration begins at bus 0 and follows the secondary-bus number of configured PCI-to-PCI bridges (class `06:04`, header type 1). A fixed 256-entry queue prevents recursion and a queued-bus bitmap prevents cycles or duplicate scans.
- Function 0 determines whether a device is multifunction; functions 1–7 are inspected only when its header advertises that capability.
- The scanner records vendor/device IDs, B/D/F location, revision, programming interface, class/subclass, header type, and interrupt line/pin. Kernel startup retains up to four class-`02` network-controller records through `asteros_pci_network_controller_count()` and `asteros_pci_network_controller_at()`.
- The generic scanner accepts an injected configuration-read callback, so tests can exercise bridge traversal and device decoding without privileged port I/O.

The scanner itself remains read-only. When startup finds a supported RTL8029AS function, the driver uses the separate resource/command API to validate its assigned BAR0 and enable only I/O decoding. Other PCI functions remain untouched. The PCI layer still does not size or assign BARs, configure bridge windows or interrupt routing, or enable bus mastering.

## Assigned BAR resources and command register

The PCI layer now provides two driver-facing helpers, both covered by synthetic host fixtures:

- `asteros_pci_read_resources()` decodes the currently programmed BARs for type-0 endpoints (six slots), type-1 PCI bridges (two slots), and type-2 CardBus bridges (one slot). It reports I/O versus memory space, the BAR index, 32- or 64-bit width, prefetchability, and the assigned base address. A 64-bit BAR consumes its following slot; unassigned zero BARs are omitted.
- The decoder rejects unsupported header layouts, below-1-MiB/reserved memory BAR encodings, truncated 64-bit BARs, and failed configuration reads. It does not write all ones to determine BAR sizes, alter a BAR, allocate an address, or infer a resource length. A reported base is only the address currently assigned by firmware/emulator.
- `asteros_pci_enable_command()` accepts only the I/O-space, memory-space, and bus-master enable bits. It ORs those bits into the existing command word, avoids redundant writes, writes only the 16-bit command register, then reads back and verifies the requested bits. The narrow write is important: the upper half of the same configuration dword is PCI status, where writing ones clears asserted status flags.
- Drivers must validate that the expected BAR exists, has the correct address-space type, and fits the device's register layout before enabling the corresponding decode bit. Bus mastering must only be enabled when the driver has valid DMA buffers and addresses. A 64-bit BAR can be decoded for inventory, but a 32-bit driver must not dereference a base above its addressable physical range.

The kernel now uses the resource reader and command helper when initializing a supported NIC. BAR sizing/assignment, MMIO mapping, bridge-window setup, and interrupt routing remain future work.

## RTL8029AS / NE2000 driver

The first concrete PCI network driver targets the Realtek RTL8029AS-compatible device (vendor/device 10EC:8029), which is the PCI NE2K device exposed by QEMU and the v86 demo selector.

- Initialization accepts only the expected PCI network-controller function and a currently assigned BAR0 that is I/O space, 32-bit, nonzero, aligned to the 0x20-byte register window, and wholly inside the 16-bit I/O-port range. It rejects missing, memory-mapped, malformed, or out-of-range BARs before requesting PCI I/O decode.
- The driver enables only the PCI I/O-space command bit. It uses programmed I/O through the NE2000 16-bit data port, so PCI bus mastering and IRQ delivery remain disabled.
- It reads the station address from the 16-bit PROM layout, configures TX page 0x40 and a bounded RX ring from pages 0x46 through 0x7F, accepts broadcast and station-addressed frames, and registers the interface as `eth0`.
- Transmit pads Ethernet frames to the 60-byte minimum and waits a bounded number of polls for PTX/TXE completion. Receive polling consumes at most one ring packet per call, validates the status/count/next-page relationship, strips the four-byte FCS, handles packets that wrap at PSTOP, and passes valid frames to the generic Ethernet II dispatcher.
- Host tests use injected port-I/O and PCI-enable callbacks; they do not execute privileged instructions. `make check-ne2k-iso` additionally boots an ISO under QEMU with `ne2k_pci` attached. This confirms the device-present boot path but does not prove that an external peer exchanged frames.

The driver remains polling-only and supports this one PCI ID. There is no VirtIO transport, IRQ handling, packet queueing, routing, DHCP, UDP/TCP, or host/v86 network relay.

## Ethernet II interface core

The kernel also builds and initializes a small hardware-independent Ethernet II core in `kernel/net.c`:

- Drivers register a caller-owned `asteros_netif_t` with a short name, unicast MAC address, MTU, link state, driver context, and synchronous transmit callback.
- The registry is fixed at four interfaces and rejects duplicate names, malformed addresses, unsupported MTUs, and missing transmit callbacks. There is no heap allocation.
- Drivers submit complete Ethernet II frames to `asteros_netif_receive_frame()`. The core checks the 14-byte header and MTU boundary, rejects IEEE 802.3 length fields for now, and dispatches the source/destination MAC addresses plus payload by EtherType to a registered protocol handler.
- Upper layers submit complete frames through `asteros_netif_send_frame()`; the core checks registration, link state, and frame bounds before calling the driver.
- Packet buffers remain owned by the caller and are consumed synchronously. The protocol consumers are a fixed-capacity ARP responder and bounded IPv4 dispatcher. There is still no packet queue, interrupt synchronization, VLAN handling, LLC/SNAP parsing, IPv4 fragment reassembly, routing, DHCP, UDP, or TCP.

The registry is initialized during `kmain()`. When a supported RTL8029AS/NE2K device is present and initializes successfully, the polling driver registers `eth0`; systems without that PCI ID simply retain an empty registry. The ARP and IPv4 modules bind a configured local address to a registered interface and consume packet buffers synchronously. The current kernel assigns `192.0.2.2` (RFC 5737 documentation space) only as a deterministic development address; there is no address negotiation, ARP cache, subnet mask, gateway, or routing. Host regression tests use mock adapters and synthetic PCI configuration fixtures; they do not require emulator hardware.

## ARP responder

`kernel/arp.c` registers EtherType `0x0806` with the existing Ethernet II dispatcher. Each configured interface has one bounded local IPv4 binding. The handler accepts only Ethernet/IPv4 ARP requests with the expected hardware/protocol lengths, a unicast nonzero sender MAC distinct from the local MAC, matching Ethernet-source and ARP-sender hardware addresses, a local or broadcast Ethernet destination, and a target protocol address matching the configured interface address. It emits a complete unicast ARP reply through `asteros_netif_send_frame()`; malformed packets, replies, other protocols, and requests for another address are ignored. Configuration rejects zero-net and class-D/E addresses. The subnet mask is not known yet, so directed-broadcast validation, ARP caching, conflict detection, and address negotiation remain future work.

The current kernel binds the first successfully registered interface to `192.0.2.2`, a documentation-only address reserved for this deterministic test path. `make check-arp` covers request construction, address binding/reconfiguration, and malformed/irrelevant packet rejection, including Ethernet/ARP sender mismatches. `make check-ne2k-peer-iso` boots the real ISO with QEMU's `ne2k_pci` device, connects a host-side TCP socket peer, injects an ARP request from `192.0.2.1`, learns the guest MAC from its reply, then sends an IPv4 ICMP echo request and verifies the guest's echo reply. The peer uses QEMU's four-byte length-prefixed socket-netdev framing; it does not attach the guest to a host or Internet network.

## IPv4 and ICMP echo

`kernel/ipv4.c` registers EtherType `0x0800` and keeps one static unicast address per registered interface plus a fixed eight-entry protocol-handler table. The receive path validates IPv4 version, IHL bounds (20–60 bytes), total length against the received payload, header checksum, nonzero TTL, source/destination addresses, and the Ethernet destination MAC. It permits DF but rejects the reserved flag, MF, and all fragment offsets; reassembly is intentionally not attempted. Ethernet padding is excluded using the IPv4 total-length field, and protocol handlers run synchronously without allocation.

`kernel/icmp.c` registers protocol 1 and responds only to valid ICMPv4 echo requests (type 8, code 0, correct checksum). Replies swap source/destination IPv4 addresses, preserve identifier/sequence/data, use the incoming Ethernet source MAC as the direct peer destination, and are sent through the existing network-interface callback. Other ICMP messages are ignored; no ICMP error generation or routing is implemented. `make check-ipv4` and `make check-icmp` cover parser boundaries, checksums, options, fragments, protocol dispatch, and echo response construction.

## Driver and backend sequence

1. [x] Add PCI configuration-space discovery and identify guest-visible network controller vendor/device IDs.
2. [x] Add assigned-BAR decoding and explicit PCI command-register enable helpers; implement the polling RTL8029AS/NE2K programmed-I/O driver against the network-interface callback contract.
3. [x] Feed received frames into the generic Ethernet II dispatcher and add a bounded ARP request/reply handler for a configured local address.
4. [x] Add bounded IPv4 validation/dispatch and an ICMPv4 echo responder with host regression tests.
5. [x] Exercise guest ARP and IPv4 ICMP echo receive/transmit with the deterministic QEMU socket peer; this validates a direct peer path, not general Internet connectivity.
6. [ ] Add subnet/gateway configuration and a bounded ARP cache before attempting routing, DHCP, UDP, or TCP.
7. [ ] Configure a v86 network backend/relay separately from the NIC selector, then add a browser-demo peer test.

The deterministic peer now proves a directly connected ARP + IPv4 ICMP echo exchange through the RTL8029/NE2K driver. Do not report general network or Internet connectivity as working: subnet/gateway configuration, routing, ARP caching, DHCP, UDP/TCP, and the v86 host backend remain absent.

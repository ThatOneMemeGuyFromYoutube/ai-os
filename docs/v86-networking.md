# v86 network-device preparation

The browser demo can expose either an emulated NE2000-compatible (NE2K) or VirtIO NIC to the AsterOS guest. The selector changes the hardware presented on the next emulator reset. It does not configure a host relay, attach a network backend, or make the guest network-capable by itself. The demo status deliberately says that the network backend is not configured.

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

The driver is intentionally polling-only and supports this one PCI ID. There is no VirtIO transport, IRQ handling, queueing, ARP/IP stack, or host/v86 network relay yet.

## Ethernet II interface core

The kernel also builds and initializes a small hardware-independent Ethernet II core in `kernel/net.c`:

- Drivers register a caller-owned `asteros_netif_t` with a short name, unicast MAC address, MTU, link state, driver context, and synchronous transmit callback.
- The registry is fixed at four interfaces and rejects duplicate names, malformed addresses, unsupported MTUs, and missing transmit callbacks. There is no heap allocation.
- Drivers submit complete Ethernet II frames to `asteros_netif_receive_frame()`. The core checks the 14-byte header and MTU boundary, rejects IEEE 802.3 length fields for now, and dispatches the payload by EtherType to a registered protocol handler.
- Upper layers submit complete frames through `asteros_netif_send_frame()`; the core checks registration, link state, and frame bounds before calling the driver.
- Packet buffers remain owned by the caller and are consumed synchronously. This first revision has no packet queue, interrupt synchronization, VLAN handling, LLC/SNAP parsing, ARP, IPv4, DHCP, UDP, or TCP.

The registry is initialized during `kmain()`. When a supported RTL8029AS/NE2K device is present and initializes successfully, the polling driver registers `eth0`; systems without that PCI ID simply retain an empty registry. Host regression tests use mock adapters and synthetic PCI configuration fixtures; they do not require emulator hardware.

## Driver and backend sequence

1. [x] Add PCI configuration-space discovery and identify guest-visible network controller vendor/device IDs.
2. [x] Add assigned-BAR decoding and explicit PCI command-register enable helpers; implement the polling RTL8029AS/NE2K programmed-I/O driver against the network-interface callback contract.
3. [ ] Feed received frames into the generic Ethernet II dispatcher and verify transmit/receive with a deterministic emulator-side peer.
4. Add protocol layers incrementally (ARP before IPv4, then ICMP/UDP and DHCP as needed) with host-testable packet parsers.
5. Configure a v86 network backend/relay separately from the NIC selector, then add an end-to-end guest connectivity test.

Do not report end-to-end networking as working until a guest protocol path and backend have been exercised with a peer. The v86 selector only chooses the emulated NIC; the RTL8029 driver currently provides Ethernet frame I/O, not IP connectivity.

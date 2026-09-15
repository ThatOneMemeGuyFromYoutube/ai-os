# v86 network-device preparation

The browser demo can expose either an NE2K or VirtIO NIC to the AsterOS guest. The selector changes the emulated hardware on reset; it does not enable host networking by itself.

v86 supports network backends separately from the NIC type, so the demo intentionally leaves relay/backend configuration out until AsterOS has a guest-side network driver and protocol stack.

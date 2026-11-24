# Architecture

AsterOS deliberately keeps the classic CP/M family separation visible:

- **Firmware/boot layer:** SeaBIOS remains the default PC firmware in QEMU and v86; an optional AsterBIOS SeaBIOS-derived ROM can be selected for emulator testing. Both firmware paths boot the same GRUB/Multiboot-compatible i386 kernel entry point.
- **Kernel:** protected-mode memory, interrupts, drivers, and process primitives.
- **BDOS-like service layer (planned):** the future stable application ABI for console, filesystem, and process operations; the current RAM-file API remains kernel-internal.
- **CCP:** command interpreter responsible for launching resident commands and transient programs.
- **Transient programs:** small user applications loaded into a defined user-memory area, with no dependency on private kernel symbols.
- **Toolchain:** host-built compiler first; later a native compiler can target the same transient-program ABI.

The project may grow a GUI, but the GUI is a client of the same service/process model rather than a replacement for it.

## Beginner experience

The intended workflow is:

```text
CREATE HELLO.C
EDIT HELLO.C
BUILD HELLO.C
RUN HELLO
```

The command vocabulary should remain small and discoverable, and examples should explain every concept before introducing advanced internals.

## Current IDE boundary

The VGA desktop has a TinyLang editor backed by a fixed 1,024-byte buffer. `kernel/fs.c` now provides an eight-slot, 8.3-name RAM filesystem with a maximum 1,023-byte file size; `kernel/ide_file.c` connects editor save/open operations to that service and refuses to replace a dirty buffer. The Files app enumerates these in-memory files. This is still an internal kernel API: contents are lost on reboot and there is no mounted filesystem block backend or user-mode BDOS syscall ABI. The kernel now probes the primary-master ATA channel with a bounded polling LBA28 PIO driver that supports single-sector reads/writes and cache flush; the Files app reports device presence. Probe is read-only, no filesystem writes are issued, and no arbitrary disk is treated as an AsterOS volume. The kernel now includes an allocation-free TinyLang syntax/label checker invoked by F5; it reports a 1-based source line and moves the editor cursor to that line. It validates the instruction, numeric, `db` string, and label grammar but does not emit machine code. The guest code generator/compiler and transient-program loader remain absent, so compile/run from the editor remains unavailable. The host Python compiler remains the reference implementation.

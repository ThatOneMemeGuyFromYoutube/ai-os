# Architecture

AsterOS deliberately keeps the classic CP/M family separation visible:

- **Boot layer:** GRUB/multiboot-compatible i386 entry point.
- **Kernel:** protected-mode memory, interrupts, drivers, and process primitives.
- **BDOS-like service layer:** the stable application ABI for console, filesystem, and process operations.
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

The VGA desktop has a TinyLang editor backed by a fixed 1,024-byte buffer. `kernel/fs.c` now provides an eight-slot, 8.3-name RAM filesystem with a maximum 1,023-byte file size; `kernel/ide_file.c` connects editor save/open operations to that service and refuses to replace a dirty buffer. The Files app enumerates these in-memory files. This is still an internal kernel API: contents are lost on reboot, no block-device/disk backend exists, and there is no user-mode BDOS syscall ABI. The guest TinyLang compiler and transient-program loader are also absent, so compile/run from the editor remains unavailable. The host compiler remains the reference implementation.

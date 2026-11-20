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

The VGA desktop has an experimental TinyLang editor backed by a fixed 1,024-byte kernel-owned buffer. It supports editing and cursor navigation but is volatile RAM only. There is not yet a BDOS file API, guest compiler, or transient-program loader, so save/load/build/run from the guest editor are intentionally not advertised. The host compiler remains the reference implementation until those OS service boundaries exist.

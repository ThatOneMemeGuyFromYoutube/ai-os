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

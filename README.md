# AsterOS

A small 32-bit x86 operating-system project inspired by the architecture and user experience of Concurrent CP/M.

## Design goals

- **CCP/M-like organization:** a resident kernel/BDOS-style service layer, a command processor (CCP), transient user programs, and a simple filesystem abstraction.
- **32-bit x86:** i386 protected mode, intended for QEMU/Bochs and old PCs.
- **Beginner friendly:** predictable commands, tiny source examples, readable C, and one-command build/run targets.
- **Programs are first-class:** the repository includes TinyLang, a deliberately small compiler that can create flat `.COM`-style 32-bit program images.
- **Self-hosting path:** TinyLang's output format is simple enough to be emitted by a future native AsterOS compiler without changing the kernel/CCP boundary.
- **Lightweight GUI shell:** a VGA text-mode desktop-like shell provides an application menu and keyboard navigation without adding a windowing stack or framebuffer dependency.

## Build prerequisites

A Unix-like host with `make`, an i386-capable GCC/binutils toolchain, GRUB utilities, and QEMU is recommended. The default Makefile accepts `CC=i686-elf-gcc` and `LD=i686-elf-ld`; a host GCC with `-m32` can be used where available.

```sh
make
make program
make run
```

`make program` demonstrates program creation without requiring a cross-compiler: it uses the small Python-based TinyLang compiler to create `build/hello.com`.

The current milestone boots into the lightweight GUI shell. Use **W/S** to navigate and **Enter** to select an application. The GUI uses the existing VGA text buffer at `0xB8000`, keeping memory and code overhead small.

## Architecture

```text
BIOS/UEFI -> GRUB -> 32-bit kernel
                    |
                    +-- BDOS-like services (console, files, process API)
                    |
                    +-- GUI shell (VGA text mode)
                    |      |
                    |      +-- application menu
                    |      +-- keyboard navigation
                    |      +-- launcher surface
                    |
                    +-- CCP command processor / transient programs
                    +-- user memory / program loader
```

The important rule is that applications talk to the OS through a small BDOS-like service interface rather than directly depending on kernel internals. The GUI is deliberately a shell layer rather than a new kernel subsystem, leaving room for multitasking, richer graphics, and a native compiler later.

## Creating programs

TinyLang is the beginner entry point:

```sh
python3 toolchain/tinylang.py my_program.tl build/my_program.com
```

The language supports a deliberately small set of 32-bit x86 operations (`mov`, `add`, `sub`, `int`, `jmp`, labels, `db`, and `exit`). This is enough to make real executable byte images now while the native/self-hosting compiler path is developed.

## Roadmap

1. Bootable 32-bit kernel + CCP shell
2. CP/M-style transient program loader and filesystem
3. **TinyLang host compiler and `.COM`-style program creation (current milestone)**
4. Load compiled programs through the CCP/BDOS boundary
5. Cooperative multitasking and safer process isolation
6. Native compiler/bootstrap compiler
7. GUI-backed terminal, files, and program launcher
8. Optional framebuffer graphics and mouse input
9. More hardware drivers and self-hosting

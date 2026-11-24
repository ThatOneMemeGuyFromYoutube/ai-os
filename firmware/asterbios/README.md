# AsterBIOS experimental firmware

AsterBIOS is an optional, AsterOS-branded legacy x86 BIOS build for the QEMU
and v86 development environments. It is deliberately separate from the
32-bit protected-mode kernel: firmware executes in the PC's reset/real-mode
environment and hands control to the existing boot media; AsterOS still enters
through GRUB and its Multiboot v1 kernel entry point.

## Compatibility policy

- SeaBIOS remains the default in QEMU, the browser demo, and release tooling.
- AsterBIOS is opt-in and must be validated against the same AsterOS ISO before
  it is offered as an alternative in the browser demo.
- The VGA BIOS remains v86's existing `vgabios.bin`; this project only builds
  the system BIOS.
- Never make AsterBIOS the default merely because its ROM builds. Require a
  successful QEMU ISO boot and v86 browser smoke test first.

## Initial implementation

The first AsterBIOS iteration is a separately built SeaBIOS 1.17.0 derivative
pinned to upstream commit
`b52ca86e094d19b58e2304417787e96b940e39c6`. This reuses the mature PC firmware
implementation rather than attempting to recreate PCI enumeration, storage
boot, ACPI, BIOS interrupts, and option-ROM handling from scratch. The build
adds the `-AsterOS` firmware version suffix, does not patch upstream firmware
runtime logic, and emits the BIOS ROM plus the exact corresponding source
archive and license texts. It tries the canonical SeaBIOS Git host first, then
the official `qemu/seabios` GitHub mirror with bounded retries; both sources
must resolve to the pinned commit. The small compatibility patch changes only
the host build probe to compile/link its alignment fixture as i386.

Build it with:

```sh
make asterbios
```

The result is `build/firmware/asterbios.bin`. The exact source archive, the `seabios.config` build profile,
`compat-test-link-i386.patch`, and the `COPYING`/`COPYING.LESSER` notices
are beside it. The profile targets the 128 KiB system-BIOS window used by v86:
it keeps QEMU/IDE, floppy, PS/2, PCI, and ACPI support while disabling USB and
non-IDE storage controllers to stay within that ROM size. The build requires Git,
GNU make, a host GCC/binutils toolchain, and Python 3 as required by SeaBIOS.
The default kernel/ISO build does not download or depend on this optional
firmware project.

Exercise the ROM with the AsterOS ISO:

```sh
make check-asterbios-iso QEMU=qemu-system-i386
make run-asterbios-iso QEMU=qemu-system-i386
```

This bounded boot check demonstrates firmware-to-GRUB-to-kernel compatibility;
it does not establish compatibility with arbitrary operating systems or
physical PCs. The reduced 128 KiB profile is intended for the project's QEMU/v86
development path, not as a drop-in replacement for a full-featured PC BIOS. No firmware binary is committed to the repository.

## Roadmap

1. Keep the exact upstream commit pinned and make the build/source/license
   artifacts reproducible; retain the verified mirror fallback, dedicated CI
   build, and QEMU boot test.
2. Add the optional firmware selector to the v86 demo, with SeaBIOS selected
   by default and AsterBIOS only selectable when the Pages build contains its
   verified ROM. Keep the v86 VGA BIOS unchanged.
3. Establish firmware regression fixtures for boot-device discovery, INT 10h
   console behavior, INT 13h disk reads, memory map/Multiboot handoff, and
   emulator-specific PCI/ACPI tables; test under QEMU and v86.
4. Make narrowly scoped AsterOS-specific firmware improvements only where
   tests demonstrate value; maintain an upstream patch series and LGPLv3
   corresponding-source distribution for every modified derivative.

## Licensing and attribution

SeaBIOS is distributed under LGPLv3 (with project files also carrying GPLv3
terms where indicated). This build does not relicense upstream code. The
build script retains the upstream source revision, includes the corresponding
source archive, and copies both upstream license files next to the ROM.
Review the exact notices and obligations before changing or redistributing
modified upstream source. See the
[SeaBIOS project](https://github.com/qemu/seabios) and its
[download/licensing notes](https://github.com/qemu/seabios/blob/master/docs/Download.md).

# AsterOS release checklist

Use this checklist before publishing a release for the 32-bit x86 AsterOS project.

## Validate the tree

```sh
make check
make all
make iso
# Optional experimental firmware validation (SeaBIOS remains default)
make check-asterbios-iso QEMU=qemu-system-i386
```

`make check` runs the host regression suites for TinyLang, the guest TinyLang syntax checker, keyboard decoding, the editor buffer, the shared application catalog, the desktop IDE event path, desktop input, PS/2 waits, Ethernet, ARP, PCI, the NE2K driver, read-only AsterOS volume discovery, and volume-scoped block-device bounds/I/O behavior, then verifies generated program examples and artifacts. `make all` builds the i386 kernel; `make iso` assembles the bootable GRUB image.

## Smoke test

Boot the generated image with QEMU using the bounded smoke targets. The peer target connects to QEMU's socket network backend and validates a guest ARP reply, rather than only checking that the NIC is present:

```sh
make check-iso QEMU=qemu-system-i386
make check-ne2k-iso QEMU=qemu-system-i386
make check-ne2k-peer-iso QEMU=qemu-system-i386
make check-volume-iso QEMU=qemu-system-i386
```

`make check-volume-iso` boots with a disposable image containing one explicitly tagged AsterOS volume descriptor; this validates the ATA read path, post-validation block-device initialization, and boot stability, not filesystem mounting or disk writes.

Run `make check-start-menu-iso QEMU=qemu-system-i386` to verify Terminal's `help files`/`help ide` topics, the `apps` inventory, Start/Run, and taskbar switching through actual guest keyboard input and VGA memory. For the IDE itself, open it from the launcher or desktop icon, edit the sample source, press F5, and verify that valid input reports success while an invalid instruction or jump moves the cursor to its 1-based diagnostic line. Also verify save/open and dirty-buffer protection. Do not treat the syntax checker as code generation, and do not treat a QEMU boot-only smoke test as proof of keyboard-driven IDE behavior.

Confirm that the kernel reaches the AsterOS shell, keyboard navigation works, and the current milestone's launcher or command path is usable.

## Publish notes

Record:

- the user-visible change;
- the roadmap area it advances;
- the exact validation commands run;
- any known limitations or follow-up work.

Keep releases small and incremental when possible so regressions are easy to identify.

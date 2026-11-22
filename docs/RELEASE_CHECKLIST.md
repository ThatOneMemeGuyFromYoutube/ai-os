# AsterOS release checklist

Use this checklist before publishing a release for the 32-bit x86 AsterOS project.

## Validate the tree

```sh
make check
make all
make iso
```

`make check` runs the host regression suites for TinyLang, the guest TinyLang syntax checker, keyboard decoding, the editor buffer, the desktop IDE event path, desktop input, PS/2 waits, Ethernet, PCI, and the NE2K driver, then verifies generated program examples and artifacts. `make all` builds the i386 kernel; `make iso` assembles the bootable GRUB image.

## Smoke test

Boot the generated image with QEMU using the bounded smoke targets:

```sh
make check-iso QEMU=qemu-system-i386
make check-ne2k-iso QEMU=qemu-system-i386
```

For interactive desktop validation, open the IDE from the launcher or desktop icon, edit the sample source, press F5, and verify that valid input reports success while an invalid instruction or jump moves the cursor to its 1-based diagnostic line. Also verify save/open and dirty-buffer protection. Do not treat the syntax checker as code generation, and do not treat a QEMU boot-only smoke test as proof of keyboard-driven IDE behavior.

Confirm that the kernel reaches the AsterOS shell, keyboard navigation works, and the current milestone's launcher or command path is usable.

## Publish notes

Record:

- the user-visible change;
- the roadmap area it advances;
- the exact validation commands run;
- any known limitations or follow-up work.

Keep releases small and incremental when possible so regressions are easy to identify.

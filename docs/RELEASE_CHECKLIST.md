# AsterOS release checklist

Use this checklist before publishing a release for the 32-bit x86 AsterOS project.

## Validate the tree

```sh
make check
make all
make iso
```

`make check` runs the TinyLang compiler tests. `make all` builds the kernel and user-space objects, while `make iso` assembles the bootable GRUB image.

## Smoke test

Boot the generated image with QEMU:

```sh
make run
```

Confirm that the kernel reaches the AsterOS shell, keyboard navigation works, and the current milestone's launcher or command path is usable.

## Publish notes

Record:

- the user-visible change;
- the roadmap area it advances;
- the exact validation commands run;
- any known limitations or follow-up work.

Keep releases small and incremental when possible so regressions are easy to identify.

# Status

## 2026-10-11 — Keyboard input foundation

The PS/2 Set 1 decoder is now a standalone, host-testable kernel module wired into the existing polling input path. It handles printable keys, Shift/Caps Lock, Escape, Tab, Enter, Backspace, and GUI up/down navigation. The new regression test runs as part of `make check`; this remains a polling decoder, not an interrupt-driven keyboard subsystem.

The GUI now labels the Files view as presentation-only and the Programs view as a built-in application inventory; the terminal identifies its shell behavior as a demo because filesystem and transient-process services are not wired yet. Keep the active README milestone open while command parsing and input ergonomics remain unfinished.

## 2026-08-26

AsterOS skeleton created. The repository now documents the intended Concurrent CP/M-inspired architecture, has a 32-bit i386 build skeleton, a multiboot-compatible entry stub, a tiny kernel/CCP placeholder, a beginner program example, and a staged path toward an in-OS compiler and self-hosting.

Next useful implementation step: establish the protected-mode runtime and keyboard/console service ABI, then turn the CCP placeholder into an interactive command processor.

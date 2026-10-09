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

The current milestone boots into the lightweight GUI shell. Use **W/S** or the arrow keys to navigate, **Enter** to select an application, **Tab** to cycle applications while a window is open, and **Esc** to close the active window. Open **Settings** from the launcher or taskbar (or press **C**) to customize pointer visibility (**V**), movement speed (**S**, cycles 1×–3×), shape (**C**), and color (**K**). Click a setting row or use its keyboard shortcut; changes apply immediately and reset on reboot.

## Browser demo

AsterOS now has a browser demo powered by [v86](https://github.com/copy/v86), an x86 emulator that runs in the browser. The GitHub Pages deployment builds a fresh AsterOS ISO, bundles the v86 browser assets, and publishes an interactive emulator with Pause, Reset, and Fullscreen controls.

After GitHub Pages is enabled for the repository, the demo is available at:

```text
https://thatonememeguyfromyoutube.github.io/ai-os/
```

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

## Development checklist

The schedule has been reset on **October 8, 2026** to recover the development time lost to repository-write access issues. Keep this schedule as the live planning source; the dates are targets, not evidence of completion. Do not mark a milestone complete just because its window has passed. Verify the implementation and tests first, then move to the next scheduled window. All work must continue to prioritize the actual repository state and the 32-bit i386/protected-mode target.

- [ ] **October 8–18 — Command line and input foundations:** make the current terminal and keyboard workflow more reliable, including Escape/Tab mappings already expected by the GUI; improve command parsing and make clear which terminal/files/program views are real services versus current shell presentation.
- [ ] **October 19–November 1 — Window system and desktop:** build on the current VGA desktop with consistent focus, open/close, taskbar, mouse, and keyboard behavior. Keep the GUI layered above the kernel and avoid introducing a larger graphics stack without a demonstrated need.
- [ ] **November 2–15 — IDE and developer tooling:** connect TinyLang compilation, useful diagnostics, examples, and a clear edit/build/run workflow. Do not call native/self-hosting support complete until the compiler actually runs as an AsterOS program.
- [ ] **November 16–29 — Networking foundation:** trace the existing v86 NIC selector into a practical guest-side plan, establish hardware-independent network interfaces where they fit the current code, and progress toward testable network features without claiming that selecting an emulated NIC provides a working network stack.
- [ ] **November 30–December 13 — Windows-98-inspired subsystem, phase I:** design and implement the first substantial compatibility/user-environment layer on top of the desktop and existing service direction, prioritizing usable shell and application conventions.
- [ ] **December 14–27 — Windows-98-inspired subsystem, phase II:** integrate real program/filesystem paths where available, round out user-facing utilities, and add cross-subsystem validation. Keep the work incremental rather than building a disconnected mock subsystem.
- [ ] **December 28–31 — Integration and stabilization:** review remaining milestone gaps, run i386 builds/tests/ISO/QEMU smoke tests, resolve regressions, refresh project status, and publish a verified release.

The active window always takes priority. If source inspection uncovers a real dependency, a more efficient implementation method, or updated GitHub Actions requirements, adjust the future schedule and add a concise engineering note when useful. Continue to improve the OS itself rather than allowing CI maintenance or documentation-only work to displace the planned feature work.

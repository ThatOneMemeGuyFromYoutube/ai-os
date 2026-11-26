# AsterOS

A small 32-bit x86 operating-system project inspired by the architecture and user experience of Concurrent CP/M.

## Design goals

- **CCP/M-like organization:** a resident kernel/BDOS-style service layer, a command processor (CCP), transient user programs, and a simple filesystem abstraction.
- **32-bit x86:** i386 protected mode, intended for QEMU/Bochs and old PCs.
- **Beginner friendly:** predictable commands, tiny source examples, readable C, and one-command build/run targets.
- **Programs are first-class:** the repository includes TinyLang, a deliberately small compiler that can create flat `.COM`-style 32-bit program images.
- **Self-hosting path:** TinyLang's output format is simple enough to be emitted by a future native AsterOS compiler without changing the kernel/CCP boundary.
- **Graphical desktop:** the current VGA text-mode shell is an interaction prototype; the planned desktop uses a validated linear framebuffer, pixel-rendered windows and icons, and a small testable 2D renderer while retaining keyboard-first access and a text fallback.

## Build prerequisites

A Unix-like host with `make`, an i386-capable GCC/binutils toolchain, GRUB utilities, and QEMU is recommended. The default Makefile accepts `CC=i686-elf-gcc` and `LD=i686-elf-ld`; a host GCC with `-m32` can be used where available.

```sh
make
make program
make run
```

`make program` demonstrates program creation without requiring a cross-compiler: it uses the small Python-based TinyLang compiler to create `build/hello.com`. Run `make examples` to compile every `toolchain/examples/*.tl` source into `build/examples/*.com`; `make check` also builds and validates the complete example set. Run `make listings` to generate a `.lst` source listing and matching `.com` image for every bundled example under `build/listings/`; `make check` exercises this target and compares the generated images with the normal example builds. To check an edited source without writing an image, run `make check-source SOURCE=my_program.tl` (or `python3 toolchain/tinylang.py --check my_program.tl`). To build it, run `make compile SOURCE=my_program.tl OUTPUT=build/my_program.com`; the output directory is created automatically. Omitting `OUTPUT` derives `build/<source-name>.com`. To also write a deterministic label map for jump targets, add `MAP=build/my_program.map`; it lists each label's 0x100-based load address and offset. Add `LISTING=build/my_program.lst` for a source-correlated listing with each line's load address and final emitted bytes (including resolved jump displacements). Both sidecars leave the executable image unchanged and can be generated together. For a complete inspection bundle in one step, run `make inspect SOURCE=my_program.tl`; it writes the `.com`, `.map`, and `.lst` together under `build/inspect/` (override `INSPECT_DIR` to choose another directory).

The current milestone boots into the lightweight GUI shell. Click **Start** or press **Ctrl+Esc** to open the Start menu; use **Up/Down** and **Enter** to launch Terminal, Files, Programs, IDE, Settings, or About. Choose **Run...** to open a built-in application by name (for example, `files`, `ide`, or `control panel`); unknown commands are rejected because a transient-program loader is not available yet. Press **Esc** to dismiss the menu or Run dialog without changing the foreground app. Run `make check-start-menu-iso` to exercise Ctrl+Esc, Run rejection/cancellation, Files launch, and Tab app switching in a real QEMU guest. PS/2 mouse setup uses bounded controller waits; if the mouse does not respond, the kernel continues with keyboard-only desktop control instead of hanging during initialization. Use **W/S** or the arrow keys to navigate, **Enter** to select an application, **Tab** to cycle applications while a window is open, and **Esc** or **Alt+F4** to close the active window. Open **Settings** from the launcher or taskbar (or press **C**) to customize pointer visibility (**V**), movement speed (**S**, cycles 1×–3×), shape (**C**), and color (**K**). Click a setting row or use its keyboard shortcut; changes apply immediately and reset on reboot. If a mouse button is unreliable, **Shift** activates the item under the pointer like a left-click. Left-button clicks use a 300 ms coalescing window: a rapid touchpad double-tap reported as two left-button pulses activates the target once, just like a physical click. This also coalesces rapid repeated left clicks from a regular mouse, and a single left click may take up to 300 ms to activate. **Right-click** or **Ctrl+Shift** opens a context menu with Open, Settings, and a target-aware Close window/Show desktop action: it closes the foreground window only when the menu targets that window; targeting the desktop or another taskbar app returns to the desktop without silently closing the foreground app. The highlighted row follows **W/S** or the arrow keys; press **Enter** or **Space** to activate it and **Esc** to dismiss the menu. Keyboard hover-click is enabled by default; open Settings and press **B** to toggle it. Each Shift press activates at most once, and the cursor position chooses the target. You can also navigate with **W/S** or the arrow keys and press **Enter** to open the selected application; Settings supports **V/S/C/K** keyboard shortcuts. Right-clicking inside an open window targets that window for **Open**. Clicking the active taskbar button minimizes its window to the desktop; clicking it again restores the same view, including terminal output and unfinished input. Switching apps with **Tab**, another taskbar button, or a context-menu action preserves the terminal output and unfinished input. Explicitly closing the **Terminal** window discards its draft and view; closing another app does not discard terminal state.

The desktop includes an experimental **IDE** application (launch with **I**, **6**, the sidebar, or its desktop icon). It opens `HELLO.TL` in a fixed 1,024-byte source editor. Use the arrow keys, **Home/End**, **Backspace/Delete**, and **Enter** to edit. Press **F5** to run the allocation-free guest-side TinyLang syntax and label checker; a failing source line is highlighted and the cursor moves to it. F5 validates source only—it does not emit a `.COM` image. **Ctrl+S** saves the current file, **Ctrl+Alt+S** opens Save As, and **Ctrl+O** opens a named file; the prompts accept uppercase/lowercase 8.3 filenames. The **Files** app lists actual RAM-filesystem entries; use **Up/Down** and **Enter** to open one in the IDE. Opening another file is refused while the editor has unsaved changes. The kernel provides a fixed-slot RAM filesystem (8 files, up to 1,023 bytes each), so save/load works during the current session, but all files are lost on reboot. Startup now probes the primary-master ATA disk with a bounded polling LBA28 PIO driver; the Files app reports whether that device was detected. The driver has single-sector read/write and cache-flush operations, but boot-time probing never writes to disk and the RAM filesystem is not yet connected to a validated on-disk volume. This is an internal kernel service, not yet a user-mode BDOS ABI. The guest still has no TinyLang code generator/compiler or transient-program loader, so compile/run from the IDE is not claimed. The host workflow remains available through `make check-source` and `make compile`.

## Browser demo

AsterOS now has a browser demo powered by [v86](https://github.com/copy/v86), an x86 emulator that runs in the browser. The GitHub Pages deployment builds a fresh AsterOS ISO, bundles the v86 browser assets, and publishes an interactive emulator with Pause, Reset, and Fullscreen controls.

After GitHub Pages is enabled for the repository, the demo is available at:

```text
https://thatonememeguyfromyoutube.github.io/ai-os/
```


### Networking status

The kernel performs read-only PCI configuration-space discovery at startup using configuration mechanism #1. It scans bus 0, follows configured PCI-to-PCI bridge secondary buses, respects multifunction headers, and records up to four class-02 network controllers. The PCI layer decodes already-assigned I/O and memory BARs (including 64-bit pairs) without probing or changing hardware; its command helper enables only requested I/O, memory, or bus-master bits with a narrow 16-bit write that preserves PCI status. The first hardware driver supports the Realtek RTL8029AS (PCI 10EC:8029) NE2000-compatible adapter: it validates the assigned I/O BAR0 before enabling I/O decoding, reads the station address from the PROM, configures a bounded receive ring, and performs polled 16-bit programmed-I/O transmit/receive. It registers an `eth0` interface with the Ethernet II core and deliberately leaves PCI bus mastering and interrupts disabled. The guest now has a fixed local development address `192.0.2.2`, a bounded ARP responder, an IPv4 receive/dispatch layer, and an ICMPv4 echo responder. `make check-ne2k-peer-iso` uses a deterministic QEMU socket peer at `192.0.2.1` to request ARP resolution, send an IPv4 ICMP echo request, and verify the guest's echo reply through the same hardware RX/TX path. This proves a directly connected L2/L3 request/reply, not general network or Internet access: there is no subnet mask, gateway, routing, ARP cache, DHCP, UDP/TCP, VirtIO driver, or configured v86 network backend. Selecting NE2K in the browser demo supplies supported hardware but still does not attach a host network backend.

## Architecture

```text
BIOS/UEFI -> GRUB -> 32-bit kernel
                    |
                    +-- kernel services (VGA console, volatile RAM file API)
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

The long-term boundary is a small BDOS-like service interface, but the current VGA shell and RAM-file API are still kernel-internal; no user-mode syscall/service ABI or transient loader exists yet. The GUI remains a shell layer rather than a new kernel subsystem. The RAM filesystem is volatile and is not a disk backend.

## Creating programs

TinyLang is the beginner entry point:

```sh
python3 toolchain/tinylang.py my_program.tl build/my_program.com
```

The language supports a deliberately small set of 32-bit x86 operations (`mov`, `add`, `sub`, `int`, `jmp`, labels, `db`, and `exit`). This is enough to make real executable byte images now while the native/self-hosting compiler path is developed. The `db` directive accepts numeric bytes and double-quoted ASCII strings with `\n`, `\r`, `\t`, `\0`, `\\`, `\"`, and `\xHH` escapes.


## Optional AsterBIOS firmware side project

AsterOS now has an opt-in, AsterOS-branded legacy BIOS build for emulator
experiments. The first iteration is based on the LGPLv3 SeaBIOS 1.17.0 source
rather than a from-scratch firmware rewrite. It builds a separately named ROM,
retains the corresponding upstream source archive and license notices, and
can be exercised with `make check-asterbios-iso QEMU=qemu-system-i386`.
The normal QEMU and v86 configurations continue using SeaBIOS by default.
The v86 VGA BIOS is unchanged. See [firmware/asterbios](firmware/asterbios/README.md)
for the build requirements, compatibility boundary, and staged roadmap.

This optional side project must not block the active kernel/desktop/toolchain
milestones. A firmware build alone is not proof of boot compatibility: keep
AsterBIOS opt-in until both QEMU and v86 have been exercised, and retain
SeaBIOS as the reliable fallback.

### AsterBIOS side-project checklist

- [x] Establish a reproducible, pinned SeaBIOS-derived AsterBIOS build with an AsterOS version suffix, ROM output, corresponding source archive, and upstream license texts.
- [x] Add a separate QEMU ISO boot target using AsterBIOS without changing the SeaBIOS default.
- [ ] Run the optional firmware build and QEMU boot target in CI; record exact source revision and validation outcome.
- [ ] Add an opt-in v86 firmware selector, defaulting to SeaBIOS; publish the custom ROM and corresponding source with the Pages demo.
- [ ] Add BIOS service and boot-contract regression coverage (boot device discovery, INT 10h/13h paths, memory map/Multiboot handoff) under QEMU and v86 before considering broader compatibility.

## Development checklist

The schedule has been reset on **October 8, 2026** to recover the development time lost to repository-write access issues. Keep this schedule as the live planning source; the dates are targets, not evidence of completion. Do not mark a milestone complete just because its window has passed. Verify the implementation and tests first, then move to the next scheduled window. All work must continue to prioritize the actual repository state and the 32-bit i386/protected-mode target.

- [x] **October 8–18 — Command line and input foundations:** make the current terminal and keyboard workflow more reliable, including Escape/Tab mappings already expected by the GUI; improve command parsing and make clear which terminal/files/program views are real services versus current shell presentation.
- [x] **October 19–November 1 — Window system and desktop (text-mode foundation):** establish focus, open/close, taskbar, mouse, and keyboard behavior in the VGA text-mode shell. This milestone delivered a usable interaction model, but its ASCII/character-cell presentation is not the final GUI.
- [ ] **REOPENED — Graphical desktop and window system (replace ASCII presentation):** the current `kernel/gui.c` draws the entire desktop, windows, borders, icons, taskbar, pointer, and IDE using 80×25 VGA text cells and ASCII glyphs. Reopen this completed milestone because it is a functional text UI, not a graphical desktop. Preserve the existing app/state/input architecture while introducing a real pixel surface; do not merely decorate the text UI with more glyphs.
  - [ ] Define a boot-time graphics contract (prefer a GRUB-provided linear framebuffer with explicit width, height, pitch, pixel format, and address; validate every field and retain a safe text-mode fallback).
  - [ ] Add a small freestanding 2D renderer (clipped pixel/fill/line/blit and bitmap-font text) with overflow-safe bounds and host-testable drawing primitives; move desktop/window rendering behind this backend without changing app semantics.
  - [ ] Replace ASCII window chrome and desktop icons with pixel-drawn panels, title bars, controls, taskbar, pointer, and actual bitmap icons; use generated PNG assets converted with a reproducible Pillow-based build step into a documented compact runtime format, with attribution/licensing recorded for any non-original artwork.
  - [ ] Port hit testing and pointer coordinates from text-cell units to framebuffer pixels, preserve keyboard accessibility and current focus/minimize/restore/context-menu behavior, and add host rendering/input regression tests plus QEMU boot and interaction checks at more than one supported resolution.
  - [ ] Only after the framebuffer path is stable, add richer window composition (resize/move, z-order/multiple visible windows, repaint/invalidation) incrementally; keep the renderer independent of the i386 kernel's device/boot details.

- [ ] **REOPENED — IDE and developer tooling (priority before further networking expansion):** this milestone was left unfinished when work moved into networking. The November 24 IPv4/ICMP work was deliberately limited to the direct-peer validation path; do not add more network protocols before the remaining IDE/storage/compiler integration receives attention. Keep the unfinished IDE work as an explicit near-term deliverable rather than treating the original November 2–15 window as closure.
  - [x] Add an allocation-free guest-side TinyLang syntax/label checker and bind it to F5 in the IDE; show the failing source line and move the editor cursor there without claiming code generation.
  - [x] Add a real editor application to the existing VGA desktop with a bounded 1,024-byte TinyLang source buffer, cursor movement, insertion/deletion, line navigation, and explicit modified state.
  - [x] Add save/load through a kernel file service with 8.3 filename prompts, Files-app listing, useful errors, and dirty-buffer protection. The current backend is an eight-slot RAM filesystem; it is session-only and is not disk persistence or a user-mode BDOS ABI.
  - [ ] Add a disk-backed storage layer and mount/boot-time file recovery without removing the RAM filesystem's bounded testable interface.
  - [x] Add a bounded polling ATA PIO driver for primary-channel LBA28 disks, including IDENTIFY, single-sector reads/writes, cache flush, host-injected I/O tests, and a disposable-disk QEMU boot check. Kernel startup only probes the primary master; it does not write to a disk.
  - [x] Define and validate an explicit AsterOS volume/partition before any filesystem write; never treat an arbitrary primary disk as safe storage. A read-only probe accepts exactly one non-overlapping primary MBR partition of type `0x7F` with an `ASTEROS1` v1 descriptor whose geometry and CRC match; it never formats or writes the disk.
  - [ ] Add a versioned, checksummed, crash-recoverable filesystem image with failure-safe mount and commit tests.
  - [ ] Connect IDE save/load to the mounted volume, preserving RAM-only behavior when no supported volume is present.
  - [ ] Extend guest-side validation into actual TinyLang code generation/compiler integration with actionable compiler diagnostics; F5 currently checks syntax and labels only and emits no program image.
  - [ ] Provide a coherent edit → validate/compile → inspect diagnostics → run workflow using existing program/service boundaries; explicitly report the unavailable guest compiler and transient loader rather than simulating success.
  - [x] Add host regression coverage for editor-buffer loading, bounds, cursor movement, RAM filesystem operations, IDE save/load, unsaved-change protection, and keyboard shortcuts.
  - [x] Add host regression coverage for TinyLang syntax, labels, byte strings, operand ranges, diagnostic line numbers, F5 scancode decoding, and editor navigation to the failing line.
  - [x] Exercise the IDE event path against a test VGA surface: F5 diagnostics and line highlight, Save As, save, dirty-buffer refusal, and reopening a file.
  - [ ] Add direct QEMU desktop interaction coverage for F5, save/open, and dirty-buffer behavior.
  - [ ] Keep native/self-hosting compiler support incomplete until the compiler actually runs as an AsterOS program.
- [x] **November 16–29 — Networking foundation (completed November 24):** the driver-neutral Ethernet II core, read-only PCI discovery, assigned-BAR handling, polling RTL8029AS/NE2K driver, and deterministic peer path are all implemented and validated. A minimal IPv4/ICMP echo path was pulled forward from the separately staged protocol work because it lets the QEMU peer prove a complete guest receive/transmit request/reply instead of stopping at ARP. This is a direct-peer test, not general network connectivity; DHCP, UDP/TCP, routing, ARP caching, and a v86 host backend remain deferred.
  - [x] Add the driver-neutral Ethernet II interface core and host regression tests.
  - [x] Add read-only PCI configuration mechanism #1 discovery, bridge traversal, multifunction enumeration, and a bounded network-controller inventory.
  - [x] Decode assigned PCI I/O and memory BARs (including 64-bit pairs) and add explicit, verified command-register enable helpers without probing or assigning BARs.
  - [x] Initialize a first RTL8029AS/NE2K driver against `asteros_netif_t`; validate the assigned I/O BAR0 before enabling I/O decoding, use polling programmed I/O, and leave bus mastering disabled.
  - [x] Add a fixed-capacity Ethernet/IPv4 ARP responder that cross-checks Ethernet and ARP sender addresses and answers only well-formed requests for the configured local interface address.
  - [x] Add bounded IPv4 receive validation and fixed-capacity protocol dispatch; check version, header/total lengths, checksum, TTL, source/destination, and reject fragments until reassembly is implemented.
  - [x] Add ICMPv4 echo replies for directly connected peers, validating the ICMP checksum and using the incoming Ethernet source MAC for the reply.
  - [x] Verify ARP and IPv4 ICMP echo receive/transmit through a deterministic QEMU socket peer with the actual RTL8029AS/NE2K driver.
- [ ] **November 30–December 13 — Windows-98-inspired subsystem, phase I:** design and implement the first substantial compatibility/user-environment layer on top of the desktop and existing service direction, prioritizing usable shell and application conventions. Work began early after the networking milestone completed on November 24.
  - [x] Replace the decorative Start taskbar button with a keyboard/mouse-accessible menu of real built-in application shortcuts; preserve the foreground window when opening or cancelling it.
  - [x] Add a bounded Run dialog with case-insensitive built-in app aliases, safe unknown-command rejection, and explicit disclosure that arbitrary program execution is unavailable until a transient loader exists.
  - [ ] Add a discoverable application/help convention that exposes each built-in app's purpose and real service limitations from the shell.
  - [x] Add direct QEMU desktop interaction coverage for Start-menu opening, Run cancellation, app launch, and taskbar switching; inspect the guest's VGA text buffer through the QEMU monitor.
- [ ] **December 14–27 — Windows-98-inspired subsystem, phase II:** integrate real program/filesystem paths where available, round out user-facing utilities, and add cross-subsystem validation. Keep the work incremental rather than building a disconnected mock subsystem.
- [ ] **December 28–31 — Integration and stabilization:** review remaining milestone gaps, run i386 builds/tests/ISO/QEMU smoke tests, resolve regressions, refresh project status, and publish a verified release.

The active window always takes priority. If source inspection uncovers a real dependency, a more efficient implementation method, or updated GitHub Actions requirements, adjust the future schedule and add a concise engineering note when useful. Continue to improve the OS itself rather than allowing CI maintenance or documentation-only work to displace the planned feature work.

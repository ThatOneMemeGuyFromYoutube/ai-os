# Status

## 2026-10-29 — Preserve terminal state across desktop focus changes

The active October 19–November 1 desktop milestone remains open. Switching away from Terminal no longer clears its last result/output, and restoring it after minimizing or using another application returns to the same terminal view and unfinished draft. Explicitly closing Terminal clears its draft and visible result; closing a different app no longer discards the terminal draft. Removed the single `minimized_app` marker because it incorrectly coupled terminal state preservation to the last minimized app. Added a host-testable close policy regression test. Build/ISO/QEMU validation and release are pending.

## 2026-10-28 — Taskbar minimize and restore

The active October 19–November 1 desktop milestone remains open. Clicking the active app's taskbar button now minimizes the single foreground view to the desktop; selecting that app again restores its previous terminal output and unfinished input. Clicking Start also returns to the desktop while retaining the minimized-app marker. Explicit close still discards the terminal draft and clears the marker. Added host-testable helpers for taskbar minimize decisions and activation output-reset policy, with regression coverage for restore versus app-switch behavior. The current CI workflows already force JavaScript actions to Node 24. Validation and release are pending for this change.

## 2026-10-27 — Context-menu window targeting and focus preservation

The active October 19–November 1 desktop milestone remains open. Context-menu Open now resolves clicks inside the foreground window to that application, rather than returning no target because normal hit testing correctly blocks click-through. App activation is centralized so clicking an already active taskbar item or reopening the same app does not clear its terminal output or unfinished input. Switching between apps with Tab, the taskbar, or context actions preserves the terminal draft; explicit window close still discards it. Added host regression tests for foreground-window context targets and focus-change detection. The existing Node 24-forced CI workflows remain current; validation and release are pending for this change.



## 2026-10-26 — Context-menu keyboard navigation

The active October 19–November 1 desktop milestone remains open. The context menu now has a visible selected row and supports Up/Down (or W/S) navigation with wraparound plus Enter/Space activation; Escape still dismisses it. Keyboard and mouse activation share the same action handler so menu behavior stays consistent. Host-testable row navigation coverage was added to `tests/test_desktop_input.c`. CI run [40384985686](https://github.com/ThatOneMemeGuyFromYoutube/ai-os/actions/runs/40384985686) passed on commit `83e2b41`: host regression tests, TinyLang checks, i386 cross-toolchain/kernel build, bootable ISO build, and QEMU ISO smoke test all succeeded. The PR-specific build/release workflow runs are still in progress; publish a new release only after the merge-triggered main build succeeds.


## 2026-10-25 — Context-menu boundary handling

The desktop context menu now uses host-testable placement and hit-test helpers in `kernel/desktop_input.c`. The menu stays inside the 80x25 VGA text grid, and clicks on its border or outside it dismiss the menu without accidentally selecting an item. Regression tests cover the three action rows, all borders, outside clicks, and bottom-right placement. Source review also caught and fixed an over-escaped NUL literal in the Open action's terminal reset.

This advances the active October 19–November 1 desktop milestone but does not complete it; focus, window lifecycle, and other desktop behavior remain. CI run 40226928274 passed host tests, i386 kernel and ISO builds, and the QEMU ISO smoke test for the code change. Local checkout is unavailable in this environment because github.com DNS resolution fails.

## 2026-10-17 — Terminal history integration and branch compatibility

Implemented the history API previously declared in `kernel/terminal.h` and switched the VGA GUI shell to use the same state object instead of maintaining separate history buffers. The input limit now reuses `ASTEROS_TERMINAL_LINE_MAX`, keeping the GUI and API buffer contracts aligned. Empty-history `!!` no longer overwrites the last-command slot. Host regression tests cover no-history repeat, successful repeat, an undersized destination that must remain unchanged, truncation to the UI limit, and clearing.

The integration passed the repository's PR workflows: host keyboard/terminal and TinyLang tests, i386 cross-toolchain checks, kernel and ISO builds, and the QEMU ISO smoke test. The active October 8–18 command/input checklist is complete; the next scheduled window is October 19–November 1 for window-system and desktop behavior.

Branch audit found no other ahead-only implementation that could be safely merged as-is: the remaining keyboard branch uses the older `keyboard_decode`/`keyboard_reset` ABI, while current `main` uses `keyboard_decode_scancode`/`keyboard_reset_state`; the old GUI branch contributes an obsolete build workflow, not a missing desktop feature. Those snapshots were not merged over newer code.

## 2026-10-12 — Terminal command parsing

The terminal now uses a standalone command parser instead of matching raw input strings inside the VGA renderer. Command names are case-insensitive, trailing whitespace is tolerated, unsupported arguments are rejected explicitly, `echo` preserves spacing within its text, and `!!` is parsed as a distinct repeat request. The `history` view snapshots the previous command before rendering, fixing the earlier self-reporting bug; `history clear` clears that snapshot. Host regression tests cover aliases, whitespace, argument handling, history commands, and unknown input.

The terminal remains a demo shell: its `pwd`, `apps/ls`, and Files/Programs views do not provide filesystem or transient-process services.

## 2026-10-11 — Keyboard input foundation

The PS/2 Set 1 decoder is now a standalone, host-testable kernel module wired into the existing polling input path. It handles printable keys, Shift/Caps Lock, Escape, Tab, Enter, Backspace, and GUI up/down navigation. The new regression test runs as part of `make check`; this remains a polling decoder, not an interrupt-driven keyboard subsystem.

The GUI now labels the Files view as presentation-only and the Programs view as a built-in application inventory; the terminal identifies its shell behavior as a demo because filesystem and transient-process services are not wired yet. Keep the active README milestone open while command parsing and input ergonomics remain unfinished.

## 2026-08-26

AsterOS skeleton created. The repository now documents the intended Concurrent CP/M-inspired architecture, has a 32-bit i386 build skeleton, a multiboot-compatible entry stub, a tiny kernel/CCP placeholder, a beginner program example, and a staged path toward an in-OS compiler and self-hosting.

Next useful implementation step: establish the protected-mode runtime and keyboard/console service ABI, then turn the CCP placeholder into an interactive command processor.

# AsterOS RAM filesystem

The current filesystem service is a small, kernel-internal RAM filesystem. It provides a real bounded file API for the IDE and Files desktop app, but it is not a disk filesystem and is not yet exposed through a user-mode BDOS syscall ABI.

## Limits and naming

- Eight fixed file slots; no heap allocation.
- Each file stores at most 1,023 bytes, matching the IDE's 1,024-byte buffer including its trailing NUL.
- Filenames use an 8.3-style form: one to eight base characters and an optional one-to-three-character extension.
- Names are canonicalized to uppercase. Accepted characters are letters, digits, underscore, and hyphen; path separators, directories, and multiple dots are rejected.
- File contents are volatile and are discarded on reboot.

## Kernel API

`kernel/fs.h` exposes:

- `asteros_fs_init()` — clear the slot table at boot.
- `asteros_fs_write()` — create or replace a file after validating the complete request.
- `asteros_fs_read()` — copy a complete file; a too-small destination is rejected without a partial copy and reports the required size.
- `asteros_fs_stat()` / `asteros_fs_count()` — enumerate occupied slots in stable slot order.
- `asteros_fs_delete()` — free a slot.
- `asteros_fs_normalize_name()` and `asteros_fs_status_message()` — shared name/error handling.

The service is synchronous and assumes the current single-core, polling kernel. It has no concurrent writer model, directories, timestamps, permissions, mount table, block-device layer, or crash recovery.

## IDE integration

`kernel/ide_file.c` connects the editor model to the RAM filesystem:

- Ctrl+S saves the current filename.
- Ctrl+Alt+S opens a Save As prompt.
- Ctrl+O opens a file prompt.
- The Files app lists actual RAM-filesystem entries; Up/Down selects and Enter opens the selected file in the IDE.
- Opening a different file is refused while the editor buffer is dirty. Save first to preserve changes.
- Source loading validates the full text before replacing the editor buffer; unsupported binary/control bytes leave the current buffer untouched.

The default `HELLO.TL` source is seeded at desktop initialization. Files saved during the session appear in Files, but reboot discards them.

## Tests and next dependencies

Run `make check-fs` for filesystem boundary tests and `make check-ide-file` for save/open integration and unsaved-change protection. Both targets are part of `make check`.

Next steps, in dependency order:

1. Add a block-device abstraction and a disk-backed filesystem or simple persistent volume while retaining the RAM service for scratch files and tests.
2. Define the user-mode BDOS file ABI and connect CCP/Files/IDE through that stable boundary.
3. Extend the guest F5 syntax/label checker into TinyLang code generation and compiler diagnostics, keeping host-compiler parity tests.
4. Implement the transient-program loader before advertising an in-OS build/run workflow.

Do not describe the current RAM filesystem as persistent storage or claim that the IDE can compile/run guest programs.

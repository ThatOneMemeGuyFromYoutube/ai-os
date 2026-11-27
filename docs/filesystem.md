# AsterOS RAM filesystem

The current filesystem service is a small, kernel-internal RAM filesystem. It provides a real bounded file API for the IDE and Files desktop app, but it is not a disk filesystem and is not yet exposed through a user-mode BDOS syscall ABI.

## Limits and naming

- Eight fixed file slots; no heap allocation.
- Each file stores at most 1,023 bytes, matching the IDE's 1,024-byte buffer including its trailing NUL.
- Filenames use an 8.3-style form: one to eight base characters and an optional one-to-three-character extension.
- Names are canonicalized to uppercase. Accepted characters are letters, digits, underscore, and hyphen; path separators, directories, and multiple dots are rejected.
- File contents are volatile and are discarded on reboot.

## Read-only AsterOS volume discovery

The kernel now distinguishes an arbitrary ATA disk from an explicitly prepared AsterOS volume before any future filesystem code is allowed to write. `kernel/volume.c` performs a read-only probe through an injected single-sector reader:

- The disk must have a valid 512-byte MBR signature (`0x55AA`).
- Exactly one primary partition must use the project-private MBR type `0x7F` (not a globally assigned partition identifier); the entry must have a valid boot flag, nonzero LBA/count, fit the detected LBA28 disk capacity, and not overlap another nonempty primary partition.
- The partition's first sector must contain the `ASTEROS1` v1 descriptor. Its fixed 36-byte header records version, sector size, flags, partition start/count, data offset, reserved field, and CRC-32 (IEEE) over bytes 0–31. The remaining bytes in the descriptor sector must be zero.
- The descriptor reserves the first partition sector and reports the remaining sectors as a future data region. It is not yet a filesystem, and the kernel does not format, mount, read file contents from, or write to that region.

`make check-volume` exercises malformed MBRs, missing/duplicate/overlapping/out-of-range partitions, descriptor version/geometry/CRC failures, and read errors. `scripts/create-volume-image.py` creates a disposable empty descriptor image for `make check-volume-iso`; the QEMU target exercises ATA reads during boot. The Files app reports whether a descriptor was recognized but continues to label all current files as volatile RAM files.

## Volume-scoped block-device service

`kernel/blockdev.h` exposes a small sector API over the data region of a previously validated `asteros_volume_t`:

- Call `asteros_blockdev_init()` only after `asteros_volume_probe()` succeeds. The initializer rechecks the partition geometry and LBA28 bounds before setting the device present.
- Callers address sectors relative to the volume data region. The MBR and the `ASTEROS1` descriptor sector are not addressable, including through writes.
- A missing write callback creates a read-only device. The kernel's ATA PIO write callback uses the existing cache-flushed single-sector operation and reports success only after the flush completes.
- `make check-blockdev` injects host read/write callbacks and covers LBA translation, first/last-sector access, out-of-range rejection without I/O, read-only mode, I/O failures, and malformed/overflowing volume geometry.

The kernel creates this block device during startup only after read-only volume discovery succeeds. It does not format or mount a filesystem and performs no boot-time disk writes. The Files app reports that the data region is ready while continuing to label editor files as volatile RAM files.

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

1. Add a versioned, checksummed disk filesystem on the volume-scoped block device, with failure-safe mount and commit tests plus boot-time recovery; never autoformat an unrecognized or blank volume.
2. Connect IDE save/load and the Files app to the mounted volume while retaining the RAM filesystem as a bounded scratch/fallback service.
3. Define the user-mode BDOS file ABI and connect CCP/Files/IDE through that stable boundary.
4. Extend the guest F5 syntax/label checker into TinyLang code generation and compiler diagnostics, keeping host-compiler parity tests.
5. Implement the transient-program loader before advertising an in-OS build/run workflow.

Do not describe the current RAM filesystem as persistent storage or claim that the IDE can compile/run guest programs.

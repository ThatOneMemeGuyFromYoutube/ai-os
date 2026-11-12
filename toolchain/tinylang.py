#!/usr/bin/env python3
"""TinyLang: a deliberately small 32-bit x86 assembler/compiler for AsterOS.

TinyLang produces CP/M-inspired flat .COM-style images: the first byte maps to
load offset 0x100. It is intentionally tiny so the format can later be emitted
by a native AsterOS compiler without changing the CCP/program boundary.

Supported statements:
    mov eax, 123
    add eax, 1
    sub eax, 1
    int 0x80
    jmp label
    label:
    db 0x90, 0x90
    exit

Labels may stand alone or precede an instruction on the same line. Numbers may
be decimal or hexadecimal (0x...). Comments start with '#' or ';'.
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
from pathlib import Path

LOAD_OFFSET = 0x100

REG_IMM = {
    "eax": 0xB8,
    "ecx": 0xB9,
    "edx": 0xBA,
    "ebx": 0xBB,
    "esp": 0xBC,
    "ebp": 0xBD,
    "esi": 0xBE,
    "edi": 0xBF,
}

JUMP_RE = re.compile(r"^jmp\s+([A-Za-z_][A-Za-z0-9_]*)$")
MOV_RE = re.compile(r"^mov\s+(eax|ecx|edx|ebx|esp|ebp|esi|edi)\s*,\s*(.+)$")
ADD_RE = re.compile(r"^add\s+eax\s*,\s*(.+)$")
SUB_RE = re.compile(r"^sub\s+eax\s*,\s*(.+)$")
INT_RE = re.compile(r"^int\s+(.+)$")
DB_RE = re.compile(r"^db(?:\s+(.*))?$")


def parse_number(token: str, line_no: int) -> int:
    token = token.strip()

    # TinyLang documents only unsigned decimal and 0x-prefixed hexadecimal.
    # Parse decimal explicitly so leading zeroes remain decimal ("0008" is 8,
    # not an invalid Python base-0 literal). Strip leading zeroes before the
    # width check so even very long zero-padded values are handled safely.
    if re.fullmatch(r"0[xX][0-9A-Fa-f]+", token):
        digits = token[2:].lstrip("0") or "0"
        if len(digits) > 8:
            raise ValueError(f"line {line_no}: number out of 32-bit range: {token}")
        value = int(digits, 16)
    elif re.fullmatch(r"[0-9]+", token):
        digits = token.lstrip("0") or "0"
        if len(digits) > 10 or (len(digits) == 10 and digits > "4294967295"):
            raise ValueError(f"line {line_no}: number out of 32-bit range: {token}")
        value = int(digits, 10)
    else:
        raise ValueError(
            f"line {line_no}: expected a decimal or hexadecimal number, got {token!r}"
        )

    return value


def emit_u32(out: bytearray, value: int) -> None:
    out.extend(struct.pack("<I", value))


def parse_db(args: str, line_no: int, out: bytearray) -> None:
    if not args.strip():
        raise ValueError(f"line {line_no}: db needs at least one byte")

    # Do not silently discard empty list items: a typo such as "db 1,,2"
    # changes the emitted image while appearing to compile successfully.
    values = [part.strip() for part in args.split(",")]
    if any(not value for value in values):
        raise ValueError(f"line {line_no}: db contains an empty byte operand")

    for token in values:
        value = parse_number(token, line_no)
        if value > 0xFF:
            raise ValueError(f"line {line_no}: db value must fit in one byte: {value}")
        out.append(value)


def diagnose_malformed_statement(line: str, line_no: int) -> None:
    """Give a syntax hint when a known instruction fails its full grammar."""
    match = re.match(r"^([A-Za-z]+)\b(.*)$", line)
    if not match:
        return

    mnemonic, operands = match.group(1), match.group(2).strip()
    if mnemonic == "mov":
        register = re.match(r"^([A-Za-z][A-Za-z0-9]*)", operands)
        if register and register.group(1) not in REG_IMM:
            supported = ", ".join(REG_IMM)
            raise ValueError(
                f"line {line_no}: unsupported mov register {register.group(1)!r}; "
                f"supported registers: {supported}"
            )
        raise ValueError(
            f"line {line_no}: malformed mov statement; "
            "expected 'mov <register>, <number>'"
        )

    if mnemonic in ("add", "sub"):
        register = re.match(r"^([A-Za-z][A-Za-z0-9]*)", operands)
        if register and register.group(1) != "eax":
            raise ValueError(
                f"line {line_no}: {mnemonic} currently supports EAX only; "
                f"expected '{mnemonic} eax, <number>'"
            )
        raise ValueError(
            f"line {line_no}: malformed {mnemonic} statement; "
            f"expected '{mnemonic} eax, <number>'"
        )

    if mnemonic == "int":
        raise ValueError(
            f"line {line_no}: malformed int statement; expected 'int <number>'"
        )

    if mnemonic == "jmp":
        raise ValueError(
            f"line {line_no}: malformed jmp statement; expected 'jmp <label>'"
        )

    if mnemonic == "db":
        raise ValueError(
            f"line {line_no}: malformed db statement; "
            "expected 'db <byte>[, <byte> ...]'"
        )

    if mnemonic == "exit":
        raise ValueError(
            f"line {line_no}: exit takes no operands; write 'exit' by itself"
        )


def record_listing_entry(entries, line_no: int, start: int, out: bytearray, raw_line: str) -> None:
    """Remember one source statement and its emitted byte range for --listing."""
    if entries is not None:
        entries.append((line_no, LOAD_OFFSET + start, start, len(out), raw_line.strip()))


def compile_source_with_symbols(source: str, listing_entries=None) -> tuple[bytes, dict[str, int]]:
    out = bytearray()
    labels: dict[str, int] = {}
    # Keep each reference's source line so second-pass diagnostics can point
    # back to the statement that introduced the unresolved jump.
    fixups: list[tuple[int, str, int, int]] = []

    for line_no, raw_line in enumerate(source.splitlines(), start=1):
        line = re.split(r"[#;]", raw_line, maxsplit=1)[0].strip()
        if not line:
            continue

        statement_start = len(out)

        # The style guide permits "label: instruction" as well as a label on
        # its own line. Record the address before assembling the instruction.
        if ":" in line:
            label, _, remainder = line.partition(":")
            label = label.strip()
            if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", label):
                raise ValueError(f"line {line_no}: invalid label {label!r}")
            if label in labels:
                raise ValueError(f"line {line_no}: duplicate label {label!r}")
            labels[label] = LOAD_OFFSET + len(out)
            line = remainder.strip()
            if not line:
                record_listing_entry(listing_entries, line_no, statement_start, out, raw_line)
                continue

        if line == "exit":
            # CLI-friendly termination primitive for the current minimal kernel:
            # halt the CPU. A real BDOS exit service will replace this later.
            out.append(0xF4)
            record_listing_entry(listing_entries, line_no, statement_start, out, raw_line)
            continue

        match = MOV_RE.match(line)
        if match:
            opcode = REG_IMM[match.group(1)]
            value = parse_number(match.group(2), line_no)
            out.append(opcode)
            emit_u32(out, value)
            record_listing_entry(listing_entries, line_no, statement_start, out, raw_line)
            continue

        match = ADD_RE.match(line)
        if match:
            out.append(0x05)  # ADD EAX, imm32
            emit_u32(out, parse_number(match.group(1), line_no))
            record_listing_entry(listing_entries, line_no, statement_start, out, raw_line)
            continue

        match = SUB_RE.match(line)
        if match:
            out.append(0x2D)  # SUB EAX, imm32
            emit_u32(out, parse_number(match.group(1), line_no))
            record_listing_entry(listing_entries, line_no, statement_start, out, raw_line)
            continue

        match = INT_RE.match(line)
        if match:
            value = parse_number(match.group(1), line_no)
            if value > 0xFF:
                raise ValueError(f"line {line_no}: interrupt vector must be 0..255")
            out.extend((0xCD, value))
            record_listing_entry(listing_entries, line_no, statement_start, out, raw_line)
            continue

        match = JUMP_RE.match(line)
        if match:
            # E9 rel32, relative to the next instruction.
            out.append(0xE9)
            fixups.append((len(out), match.group(1),
                           LOAD_OFFSET + len(out) + 4, line_no))
            out.extend(b"\x00\x00\x00\x00")
            record_listing_entry(listing_entries, line_no, statement_start, out, raw_line)
            continue

        match = DB_RE.match(line)
        if match:
            parse_db(match.group(1) or "", line_no, out)
            record_listing_entry(listing_entries, line_no, statement_start, out, raw_line)
            continue

        diagnose_malformed_statement(line, line_no)
        raise ValueError(f"line {line_no}: unknown statement: {line!r}")

    for patch_at, label, next_ip, line_no in fixups:
        if label not in labels:
            raise ValueError(f"line {line_no}: unknown label: {label!r}")
        relative = labels[label] - next_ip
        if not -(1 << 31) <= relative < (1 << 31):
            raise ValueError(f"line {line_no}: jump to {label!r} is out of range")
        out[patch_at : patch_at + 4] = struct.pack("<i", relative)

    return bytes(out), labels


def compile_source(source: str) -> bytes:
    image, _ = compile_source_with_symbols(source)
    return image


def format_symbol_map(labels: dict[str, int]) -> str:
    """Render deterministic load addresses and image offsets for labels."""
    lines = [
        "# TinyLang symbol map",
        f"# load base: 0x{LOAD_OFFSET:08X}",
        "# address    offset     symbol",
    ]
    for name, address in sorted(labels.items(), key=lambda item: (item[1], item[0])):
        lines.append(f"0x{address:08X} 0x{address - LOAD_OFFSET:08X} {name}")
    return "\n".join(lines) + "\n"


def format_source_listing(entries, image: bytes) -> str:
    """Render source lines with their final load addresses and emitted bytes."""
    lines = [
        "# TinyLang source listing",
        f"# load base: 0x{LOAD_OFFSET:08X}",
        "# line  address    bytes | source",
    ]
    for line_no, address, start, end, source in entries:
        encoded = " ".join(f"{value:02X}" for value in image[start:end])
        lines.append(f"{line_no:04d} 0x{address:08X}  {encoded} | {source}")
    return "\n".join(lines) + "\n"


def paths_refer_to_same_file(left: Path, right: Path) -> bool:
    """Detect identical paths, symlink aliases, and existing hard-link aliases."""
    try:
        if left.resolve() == right.resolve():
            return True
        return left.samefile(right)
    except (OSError, RuntimeError):
        return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile TinyLang to a flat AsterOS .COM-style image.")
    parser.add_argument("source", type=Path, help="TinyLang source file")
    parser.add_argument(
        "output", type=Path, nargs="?", help="output .com image (not used with --check)"
    )
    parser.add_argument(
        "--check", action="store_true",
        help="validate syntax and labels without writing a program image",
    )
    parser.add_argument(
        "--map", type=Path, metavar="PATH",
        help="write a deterministic label-address map alongside the program image",
    )
    parser.add_argument(
        "--listing", type=Path, metavar="PATH",
        help="write a source-correlated listing of final emitted bytes",
    )
    args = parser.parse_args()

    if args.check and args.output is not None:
        parser.error("--check does not accept an output path")
    if args.check and args.map is not None:
        parser.error("--map cannot be used with --check")
    if args.check and args.listing is not None:
        parser.error("--listing cannot be used with --check")
    if not args.check and args.output is None:
        parser.error("output is required unless --check is used")
    if args.output is not None and paths_refer_to_same_file(args.source, args.output):
        parser.error("output path must differ from source path")
    if args.map is not None:
        if args.output is not None and paths_refer_to_same_file(args.map, args.output):
            parser.error("map path must differ from output path")
        if paths_refer_to_same_file(args.map, args.source):
            parser.error("map path must differ from source path")
    if args.listing is not None:
        if args.output is not None and paths_refer_to_same_file(args.listing, args.output):
            parser.error("listing path must differ from output path")
        if args.map is not None and paths_refer_to_same_file(args.listing, args.map):
            parser.error("listing path must differ from map path")
        if paths_refer_to_same_file(args.listing, args.source):
            parser.error("listing path must differ from source path")

    try:
        source = args.source.read_text(encoding="utf-8")
        listing_entries = [] if args.listing is not None else None
        image, labels = compile_source_with_symbols(source, listing_entries)
        if args.check:
            print(f"tinylang: {args.source} is valid ({len(image)} bytes)")
            return 0
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(image)
        if args.map is not None:
            args.map.parent.mkdir(parents=True, exist_ok=True)
            args.map.write_text(format_symbol_map(labels), encoding="utf-8")
        if args.listing is not None:
            args.listing.parent.mkdir(parents=True, exist_ok=True)
            args.listing.write_text(format_source_listing(listing_entries, image), encoding="utf-8")
    except (OSError, ValueError) as exc:
        print(f"tinylang: {exc}", file=sys.stderr)
        return 1

    message = f"tinylang: wrote {len(image)} bytes to {args.output}"
    if args.map is not None:
        message += f"; symbol map: {args.map}"
    if args.listing is not None:
        message += f"; source listing: {args.listing}"
    print(message)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

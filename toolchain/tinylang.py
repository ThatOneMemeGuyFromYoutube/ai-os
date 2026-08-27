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

Numbers may be decimal or hexadecimal (0x...). Comments start with '#'.
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
DB_RE = re.compile(r"^db\s+(.+)$")


def parse_number(token: str, line_no: int) -> int:
    token = token.strip()
    try:
        value = int(token, 0)
    except ValueError as exc:
        raise ValueError(f"line {line_no}: expected a number, got {token!r}") from exc
    if not 0 <= value <= 0xFFFFFFFF:
        raise ValueError(f"line {line_no}: number out of 32-bit range: {value}")
    return value


def emit_u32(out: bytearray, value: int) -> None:
    out.extend(struct.pack("<I", value))


def parse_db(args: str, line_no: int, out: bytearray) -> None:
    values = [part.strip() for part in args.split(",") if part.strip()]
    if not values:
        raise ValueError(f"line {line_no}: db needs at least one byte")
    for token in values:
        value = parse_number(token, line_no)
        if value > 0xFF:
            raise ValueError(f"line {line_no}: db value must fit in one byte: {value}")
        out.append(value)


def compile_source(source: str) -> bytes:
    out = bytearray()
    labels: dict[str, int] = {}
    fixups: list[tuple[int, str, int]] = []

    for line_no, raw_line in enumerate(source.splitlines(), start=1):
        line = raw_line.split("#", 1)[0].strip()
        if not line:
            continue

        if line.endswith(":"):
            label = line[:-1].strip()
            if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", label):
                raise ValueError(f"line {line_no}: invalid label {label!r}")
            if label in labels:
                raise ValueError(f"line {line_no}: duplicate label {label!r}")
            labels[label] = LOAD_OFFSET + len(out)
            continue

        if line == "exit":
            # CLI-friendly termination primitive for the current minimal kernel:
            # halt the CPU. A real BDOS exit service will replace this later.
            out.append(0xF4)
            continue

        match = MOV_RE.match(line)
        if match:
            opcode = REG_IMM[match.group(1)]
            value = parse_number(match.group(2), line_no)
            out.append(opcode)
            emit_u32(out, value)
            continue

        match = ADD_RE.match(line)
        if match:
            out.append(0x05)  # ADD EAX, imm32
            emit_u32(out, parse_number(match.group(1), line_no))
            continue

        match = SUB_RE.match(line)
        if match:
            out.append(0x2D)  # SUB EAX, imm32
            emit_u32(out, parse_number(match.group(1), line_no))
            continue

        match = INT_RE.match(line)
        if match:
            value = parse_number(match.group(1), line_no)
            if value > 0xFF:
                raise ValueError(f"line {line_no}: interrupt vector must be 0..255")
            out.extend((0xCD, value))
            continue

        match = JUMP_RE.match(line)
        if match:
            # E9 rel32, relative to the next instruction.
            out.append(0xE9)
            fixups.append((len(out), match.group(1), LOAD_OFFSET + len(out) + 4))
            out.extend(b"\x00\x00\x00\x00")
            continue

        match = DB_RE.match(line)
        if match:
            parse_db(match.group(1), line_no, out)
            continue

        raise ValueError(f"line {line_no}: unknown statement: {line!r}")

    for patch_at, label, next_ip in fixups:
        if label not in labels:
            raise ValueError(f"unknown label: {label!r}")
        relative = labels[label] - next_ip
        if not -(1 << 31) <= relative < (1 << 31):
            raise ValueError(f"jump to {label!r} is out of range")
        out[patch_at : patch_at + 4] = struct.pack("<i", relative)

    return bytes(out)


def main() -> int:
    parser = argparse.ArgumentParser(description="Compile TinyLang to a flat AsterOS .COM-style image.")
    parser.add_argument("source", type=Path, help="TinyLang source file")
    parser.add_argument("output", type=Path, help="output .com image")
    args = parser.parse_args()

    try:
        source = args.source.read_text(encoding="utf-8")
        image = compile_source(source)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(image)
    except (OSError, ValueError) as exc:
        print(f"tinylang: {exc}", file=sys.stderr)
        return 1

    print(f"tinylang: wrote {len(image)} bytes to {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

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
    match = re.match(r"^([A-Za-z]+)\\b(.*)$", line)
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


def compile_source(source: str) -> bytes:
    out = bytearray()
    labels: dict[str, int] = {}
    # Keep each reference's source line so second-pass diagnostics can point
    # back to the statement that introduced the unresolved jump.
    fixups: list[tuple[int, str, int, int]] = []

    for line_no, raw_line in enumerate(source.splitlines(), start=1):
        line = re.split(r"[#;]", raw_line, maxsplit=1)[0].strip()
        if not line:
            continue

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
            fixups.append((len(out), match.group(1),
                           LOAD_OFFSET + len(out) + 4, line_no))
            out.extend(b"\x00\x00\x00\x00")
            continue

        match = DB_RE.match(line)
        if match:
            parse_db(match.group(1) or "", line_no, out)
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

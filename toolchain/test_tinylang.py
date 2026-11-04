#!/usr/bin/env python3
"""Small dependency-free regression tests for TinyLang."""

from tinylang import compile_source


def expect_error(source: str, *fragments: str) -> None:
    try:
        compile_source(source)
    except ValueError as exc:
        message = str(exc)
        for fragment in fragments:
            assert fragment in message, f"expected {fragment!r} in {message!r}"
    else:
        raise AssertionError(f"expected compilation to fail for {source!r}")


def main() -> None:
    image = compile_source("""
        start: ; labels may be followed by semicolon comments
            mov eax, 1 ; inline comments are ignored
            add eax, 2
            jmp done
            db 0x90
        done:
            exit
    """)
    assert image[:5] == bytes([0xB8, 1, 0, 0, 0])
    assert image[5:10] == bytes([0x05, 2, 0, 0, 0])
    assert image[-1:] == b"\xF4"

    # A label and its instruction may share a line. Forward jump fixups must
    # still use the address immediately after the five-byte JMP rel32.
    inline_labels = compile_source("""
        start: mov eax, 1
        loop: sub eax, 1
        jmp done
        db 0x90
        done: exit
    """)
    assert inline_labels[:5] == bytes([0xB8, 1, 0, 0, 0])
    assert inline_labels[5:10] == bytes([0x2D, 1, 0, 0, 0])
    assert inline_labels[10:15] == bytes([0xE9, 1, 0, 0, 0])
    assert inline_labels[15:] == b"\x90\xF4"

    # Numeric syntax is deliberately decimal or 0x-prefixed hexadecimal.
    # Leading zeroes do not turn a decimal literal into octal.
    numeric_literals = compile_source("""
        mov eax, 0008
        add eax, 0009
        sub eax, 0X0A
        int 000128
        db 0007, 0Xff
        exit
    """)
    assert numeric_literals == bytes([
        0xB8, 8, 0, 0, 0,
        0x05, 9, 0, 0, 0,
        0x2D, 10, 0, 0, 0,
        0xCD, 128,
        7, 0xFF, 0xF4,
    ])
    expect_error("mov eax, -1", "line 1", "expected a decimal or hexadecimal number")
    expect_error("add eax, 0b10", "line 1", "expected a decimal or hexadecimal number")
    expect_error("sub eax, 1_0", "line 1", "expected a decimal or hexadecimal number")
    expect_error("mov eax, 4294967296", "line 1", "out of 32-bit range")
    expect_error("mov eax, 0x100000000", "line 1", "out of 32-bit range")

    # Empty db operands must not be silently filtered out, because that
    # would emit a different program than the source appears to describe.
    assert compile_source("db 0x00, 0x7F, 0xFF") == bytes([0x00, 0x7F, 0xFF])
    expect_error("db", "line 1", "db needs at least one byte")
    expect_error("db 1,,2", "line 1", "empty byte operand")
    expect_error("db 1,", "line 1", "empty byte operand")
    expect_error("db ,1", "line 1", "empty byte operand")
    expect_error("mov eax, 1\n db 0x10, ,0x20", "line 2", "empty byte operand")

    expect_error("jmp missing", "line 1", "unknown label")
    expect_error("start: mov eax, 1\n  jmp missing", "line 2", "unknown label")
    expect_error("same: exit\nsame: exit", "line 2", "duplicate label")

    print("tinylang: tests passed")


if __name__ == "__main__":
    main()

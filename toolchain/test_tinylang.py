#!/usr/bin/env python3
"""Small dependency-free regression tests for TinyLang."""

from tinylang import compile_source


def main() -> None:
    image = compile_source("""
        start:
            mov eax, 1
            add eax, 2
            jmp done
            db 0x90
        done:
            exit
    """)
    assert image[:5] == bytes([0xB8, 1, 0, 0, 0])
    assert image[5:10] == bytes([0x05, 2, 0, 0, 0])
    assert image[-1:] == b"\xF4"

    try:
        compile_source("jmp missing")
    except ValueError as exc:
        assert "unknown label" in str(exc)
    else:
        raise AssertionError("missing labels must fail")

    print("tinylang: tests passed")


if __name__ == "__main__":
    main()

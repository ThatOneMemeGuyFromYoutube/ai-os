#!/usr/bin/env python3
"""Small dependency-free regression tests for TinyLang."""

from pathlib import Path
import subprocess
import sys
import tempfile

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


def test_example_programs() -> None:
    examples_dir = Path(__file__).with_name("examples")
    example_paths = sorted(examples_dir.glob("*.tl"))
    assert len(example_paths) >= 3, "expected the beginner example collection"

    for source_path in example_paths:
        image = compile_source(source_path.read_text(encoding="utf-8"))
        assert image, f"{source_path.name} produced an empty image"
        assert image[-1] == 0xF4, f"{source_path.name} must end with TinyLang exit"

    arithmetic = compile_source(
        (examples_dir / "arithmetic.tl").read_text(encoding="utf-8")
    )
    assert arithmetic == bytes([
        0xB8, 8, 0, 0, 0,
        0x05, 16, 0, 0, 0,
        0x2D, 2, 0, 0, 0,
        0xF4,
    ])

    jump_over_data = compile_source(
        (examples_dir / "jump_over_data.tl").read_text(encoding="utf-8")
    )
    assert jump_over_data == bytes([
        0xE9, 7, 0, 0, 0,
        0x41, 0x73, 0x74, 0x65, 0x72, 0x4F, 0x53,
        0xB8, 42, 0, 0, 0,
        0xF4,
    ])



def test_cli_check_mode() -> None:
    compiler = Path(__file__).with_name("tinylang.py")
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        valid_source = directory / "valid.tl"
        valid_source.write_text("mov eax, 42\nexit\n", encoding="utf-8")

        checked = subprocess.run(
            [sys.executable, str(compiler), "--check", str(valid_source)],
            capture_output=True, text=True, check=False,
        )
        assert checked.returncode == 0, checked.stderr
        assert "valid.tl is valid (6 bytes)" in checked.stdout
        assert sorted(path.name for path in directory.iterdir()) == ["valid.tl"]

        invalid_source = directory / "invalid.tl"
        invalid_source.write_text("mov eax, 1\njmp missing\n", encoding="utf-8")
        rejected = subprocess.run(
            [sys.executable, str(compiler), "--check", str(invalid_source)],
            capture_output=True, text=True, check=False,
        )
        assert rejected.returncode == 1
        assert "line 2: unknown label" in rejected.stderr
        assert sorted(path.name for path in directory.iterdir()) == [
            "invalid.tl", "valid.tl"
        ]

        output = directory / "nested" / "valid.com"
        built = subprocess.run(
            [sys.executable, str(compiler), str(valid_source), str(output)],
            capture_output=True, text=True, check=False,
        )
        assert built.returncode == 0, built.stderr
        assert output.read_bytes() == bytes([0xB8, 42, 0, 0, 0, 0xF4])

        invalid_usage = subprocess.run(
            [sys.executable, str(compiler), "--check",
             str(valid_source), str(directory / "ignored.com")],
            capture_output=True, text=True, check=False,
        )
        assert invalid_usage.returncode == 2
        assert "--check does not accept an output path" in invalid_usage.stderr
        assert not (directory / "ignored.com").exists()


def test_cli_symbol_map() -> None:
    compiler = Path(__file__).with_name("tinylang.py")
    with tempfile.TemporaryDirectory() as temporary:
        directory = Path(temporary)
        source = directory / "labels.tl"
        source.write_text(
            "zeta:\nalpha:\nstart: jmp done\ndata: db 0x90\ndone: exit\n",
            encoding="utf-8",
        )
        output = directory / "nested" / "program.com"
        map_path = directory / "nested" / "program.map"

        built = subprocess.run(
            [sys.executable, str(compiler), "--map", str(map_path),
             str(source), str(output)],
            capture_output=True, text=True, check=False,
        )
        assert built.returncode == 0, built.stderr
        assert output.read_bytes() == bytes([0xE9, 1, 0, 0, 0, 0x90, 0xF4])
        assert map_path.read_text(encoding="utf-8") == (
            "# TinyLang symbol map\n"
            "# load base: 0x00000100\n"
            "# address    offset     symbol\n"
            "0x00000100 0x00000000 alpha\n"
            "0x00000100 0x00000000 start\n"
            "0x00000100 0x00000000 zeta\n"
            "0x00000105 0x00000005 data\n"
            "0x00000106 0x00000006 done\n"
        )
        assert "symbol map:" in built.stdout

        checked_with_map = subprocess.run(
            [sys.executable, str(compiler), "--check", "--map",
             str(map_path), str(source)],
            capture_output=True, text=True, check=False,
        )
        assert checked_with_map.returncode == 2
        assert "--map cannot be used with --check" in checked_with_map.stderr
        assert map_path.is_file()

        collision = subprocess.run(
            [sys.executable, str(compiler), "--map", str(output),
             str(source), str(output)],
            capture_output=True, text=True, check=False,
        )
        assert collision.returncode == 2
        assert "map path must differ from output path" in collision.stderr
        assert output.read_bytes() == bytes([0xE9, 1, 0, 0, 0, 0x90, 0xF4])

        broken = directory / "broken.tl"
        broken.write_text("jmp missing\n", encoding="utf-8")
        failed_output = directory / "failed" / "program.com"
        failed_map = directory / "failed" / "program.map"
        failed = subprocess.run(
            [sys.executable, str(compiler), "--map", str(failed_map),
             str(broken), str(failed_output)],
            capture_output=True, text=True, check=False,
        )
        assert failed.returncode == 1
        assert "line 1: unknown label" in failed.stderr
        assert not failed_output.exists()
        assert not failed_map.exists()


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

    # Recognized instructions should explain their required operand shape
    # instead of falling through to the generic unknown-statement message.
    expect_error("mov eax", "line 1", "malformed mov", "mov <register>, <number>")
    expect_error("mov eax,", "line 1", "malformed mov")
    expect_error("mov foo, 1", "line 1", "unsupported mov register", "foo")
    expect_error("mov eax 1", "line 1", "malformed mov")
    expect_error("add ebx, 1", "line 1", "supports EAX only")
    expect_error("add eax", "line 1", "malformed add", "add eax, <number>")
    expect_error("sub eax,", "line 1", "malformed sub")
    expect_error("int", "line 1", "malformed int", "int <number>")
    expect_error("jmp", "line 1", "malformed jmp", "jmp <label>")
    expect_error("jmp 0x100", "line 1", "malformed jmp")
    expect_error("db, 1", "line 1", "malformed db", "db <byte>")
    expect_error("exit 1", "line 1", "exit takes no operands")
    expect_error("mov eax, 1\nadd ebx, 2", "line 2", "supports EAX only")

    test_example_programs()
    test_cli_check_mode()
    test_cli_symbol_map()
    print("tinylang: tests passed")


if __name__ == "__main__":
    main()

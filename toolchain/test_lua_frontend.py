#!/usr/bin/env python3
"""Regression tests for the Lua-adjacent TinyLang source frontend."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from tinylang import compile_source
from lua_frontend import lower_lua_source

def expect_error(source, fragment):
    try:
        compile_source(source)
    except ValueError as error:
        assert fragment in str(error), str(error)
    else:
        raise AssertionError(f"expected {fragment!r} for {source!r}")

def main():
    examples = Path(__file__).with_name("examples")
    assert compile_source((examples / "hello.tl").read_text()) == bytes([
        0xB8, 42, 0, 0, 0, 0x05, 1, 0, 0, 0, 0xF4])
    assert compile_source((examples / "arithmetic.tl").read_text()) == bytes([
        0xB8, 8, 0, 0, 0, 0x05, 16, 0, 0, 0, 0x2D, 2, 0, 0, 0, 0xF4])
    multi = "local first = 5\nlocal second = 7\nsecond = second + 3\nsecond = second - 1\nreturn\n"
    assert compile_source(multi) == bytes([
        0xB8, 5, 0, 0, 0, 0xB9, 7, 0, 0, 0,
        0x81, 0xC1, 3, 0, 0, 0, 0x81, 0xE9, 1, 0, 0, 0, 0xF4])
    implicit_return = "local value = 9 -- comment\nvalue = 10;\n"
    assert compile_source(implicit_return)[-1] == 0xF4
    assert lower_lua_source("-- comment only\nlocal value = 1\nreturn\n").endswith("exit\n")
    expect_error("local x = 1\nx = x + -1", "integer literal")
    expect_error("local x = 1\nlocal x = 2", "duplicate local")
    expect_error("x = 1", "unknown variable")
    expect_error("local x = 1\nx = y + 1", "expressions must update their own variable")
    expect_error("local x = 4294967296", "exceeds 32 bits")
    expect_error("local x = 1\nreturn\nx = 2", "no statements may follow return")
    expect_error("local x = 1\nlocal y = 2\nlocal z = 3\nlocal a = 4\nlocal b = 5\nlocal c = 6\nlocal d = 7", "at most six")
    print("tinylang-lua: tests passed")

if __name__ == "__main__":
    main()

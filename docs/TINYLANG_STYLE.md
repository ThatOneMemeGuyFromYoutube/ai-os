# TinyLang source style

TinyLang emits flat 32-bit x86 `.COM`-style program images for AsterOS. Keep source small, explicit, and easy to inspect.

## Labels

Labels may stand alone or share a line with an instruction:

```text
start:
    mov eax, 1

loop: sub eax, 1
      jmp loop
```

Use labels for control-flow targets instead of hard-coded offsets. Labels are case-sensitive and may contain letters, digits, and underscores.

## Numeric operands

Use unsigned decimal or `0x`-prefixed hexadecimal literals. Leading zeroes do not select octal:

```text
mov eax, 0008   # decimal eight
add eax, 0x10   # hexadecimal sixteen
int 0x80
db 0, 0x7F, 0xFF
```

Immediate values must fit in 32 bits; `db` values and interrupt vectors must fit in one byte. Signed values, binary prefixes, and digit separators are not part of the current language.

## Comments

Use `#` or `;` for comments. Inline comments are supported:

```text
mov eax, 1 ; initialize the accumulator
```

## Validation

Run the repository regression test before packaging a program:

```sh
PYTHONPATH=toolchain python3 toolchain/test_tinylang.py
```

Then build the 32-bit artifacts with `make check` before publishing a release.

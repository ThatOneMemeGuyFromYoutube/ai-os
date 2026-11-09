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

## Instruction forms

Keep each instruction's operands in the documented order and include the comma
between a register and an immediate value:

```text
mov eax, 42
add eax, 1
sub eax, 1
int 0x80
jmp done
db 0x90, 0xCC
exit
```

`add` and `sub` currently operate on EAX only. `mov` accepts the eight 32-bit general-purpose registers listed by the compiler. `exit` takes no operands. If a known instruction is malformed, TinyLang reports its expected operand form and the source line.

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

## Symbol maps

When building a program, pass `--map PATH` to the compiler or `MAP=PATH`
to `make compile`. The generated text file lists labels by load address,
including both the current 0x100-based address and the offset within the flat
image. It is useful when checking relative-jump targets; it does not alter the
program image. Map output is unavailable in `--check` mode because validation
does not write build artifacts.

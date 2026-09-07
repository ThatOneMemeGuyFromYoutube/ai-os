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

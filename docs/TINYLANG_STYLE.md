# TinyLang source style

TinyLang is a small, Lua-adjacent scripting language for AsterOS. Write source
code, not CPU instructions. The host compiler currently lowers this beginner
subset to the existing flat 32-bit i386 program image.

## Current source language

```lua
-- Local values and familiar arithmetic
local answer = 42
answer = answer + 1

local total = 8
total = total + 0x10
total = total - 2

return
```

- `local name = integer` declares a local scalar.
- `name = integer` assigns a new integer value.
- `name = name + integer` and `name = name - integer` update a local.
- Integer literals are unsigned decimal or `0x` hexadecimal and must fit in 32 bits.
- Names are case-sensitive identifiers beginning with a letter or underscore.
- Up to six locals are currently supported.
- A program may end with `return`; otherwise the compiler adds an implicit return.
- `--` begins a line comment. A trailing semicolon is optional.

This is deliberately a first language slice, not full Lua. Expressions are
currently integer literals or self-updates; strings, function calls, tables,
boolean values, comparisons, `if`, `while`, and user-defined functions are
not implemented yet. Unsupported syntax receives a source-line diagnostic.

## Compiler and output

`python3 toolchain/tinylang.py source.tl build/program.com` compiles source
into a flat 32-bit x86 image with load offset 0x100. The x86 instructions are
compiler implementation details, not the TinyLang programming interface.
`make check-source SOURCE=source.tl` validates without writing output;
`make compile SOURCE=source.tl OUTPUT=build/program.com` builds the image.

The old instruction-oriented syntax remains accepted temporarily as a
compatibility backend for historical jump/data examples. New examples and
new programs should use the Lua-adjacent syntax above. Do not add assembly
mnemonics to beginner-facing examples.

## IDE validation

The guest F5 checker currently validates the historical instruction grammar.
It must be brought into parity with the Lua-adjacent source parser before the
IDE can claim that F5 validates the new language. Until then, use the host
`make check-source` or `make compile` workflow for new-style source.

## Regression tests

Run `PYTHONPATH=toolchain python3 toolchain/test_tinylang.py` and
`PYTHONPATH=toolchain python3 toolchain/test_lua_frontend.py`. Both are part
of `make check`.

# TinyLang toolchain

TinyLang is AsterOS's beginner scripting language. Its source is **Lua-adjacent,
not assembly**: users write local variables, assignments, integer arithmetic,
Lua-style comments, and return statements. The compiler lowers that source to
the project's existing flat 32-bit i386 program image.

## Try it

```sh
make program
make check-source SOURCE=toolchain/examples/hello.tl
make compile SOURCE=toolchain/examples/hello.tl OUTPUT=build/hello.com
```

A source file looks like this:

```lua
-- A small AsterOS program
local answer = 42
answer = answer + 1
return
```

The first supported language slice includes `local name = integer`,
assignments to declared locals, self-updating `name = name + integer` and
`name = name - integer`, optional trailing semicolons, `--` comments, and
`return`. Decimal and `0x` hexadecimal literals are accepted. Up to six
locals are supported. Programs without an explicit return receive an implicit
one.

This is not full Lua yet. Strings, `print`, function calls, tables, boolean
values, comparisons, `if`, `while`, and user-defined functions are future
language work. The current kernel has no stable console BDOS ABI, so a `print`
builtin would be misleading today.

## Compilation boundary

The public language is parsed by `toolchain/lua_frontend.py`. It lowers to a
private code-generation representation consumed by `toolchain/tinylang.py`.
That backend emits the existing flat i386 image; source authors do not write
or maintain the generated instruction representation. The output format and
future CCP loader boundary remain unchanged.

The historical instruction syntax remains temporarily accepted for legacy
jump/data examples and regression fixtures. New examples must use the Lua-like
surface syntax. The guest IDE's F5 checker still understands the old grammar;
porting it to the new language is a tracked follow-up, so host validation is
authoritative for Lua-adjacent source at this stage.

## Tests

`make check` runs both the historical backend tests and the new language
frontend tests. They verify emitted i386 bytes, local-variable register
allocation, arithmetic updates, comments, implicit/explicit return, and
source-line diagnostics for invalid declarations and assignments.

TinyLang is host-compiled today. A guest-side compiler and transient-program
loader are still absent; do not claim in-OS compile/run or self-hosting.

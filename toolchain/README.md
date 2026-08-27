# TinyLang toolchain

TinyLang is the first concrete program-building tool for AsterOS. It is intentionally small and host-buildable today so beginners can experiment without first needing to build a complete cross-compiler.

## Try it

From the repository root:

```sh
make program
```

That creates `build/hello.com`, a flat 32-bit x86 program image. You can also compile your own source:

```sh
python3 toolchain/tinylang.py my_program.tl build/my_program.com
```

A TinyLang program can use `mov`, `add`, `sub`, `int`, `jmp`, labels, `db`, and `exit`. Comments begin with `#`. See `examples/hello.tl` for the smallest example.

## Why this exists

The output is a simple CP/M-inspired `.COM`-style image intended for the future CCP transient-program loader. Keeping the program format simple gives AsterOS a clear path from a host compiler to a compiler that runs as an AsterOS transient program, without changing the kernel/BDOS/CCP boundary.

## Roadmap

1. TinyLang source language and flat program format (current)
2. Load TinyLang `.com` images through the CCP/program loader
3. Add more useful language features and BDOS service calls
4. Port the compiler to AsterOS as a transient program
5. Bootstrap the native compiler from its own source

TinyLang is a development tool today; it is not yet a self-hosting compiler.

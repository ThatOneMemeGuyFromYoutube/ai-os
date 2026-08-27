# Toolchain

The first implementation can use an external i386 cross-compiler. The native/self-hosting path will add a small compiler that runs as an AsterOS transient program and emits the same program format consumed by the CCP loader.

This directory is intentionally a placeholder rather than pretending the compiler already exists.

Planned stages:

1. Tiny source language and bytecode/intermediate format.
2. Host compiler that emits the format.
3. Native compiler port running inside AsterOS.
4. Compiler bootstrap: rebuild the native compiler from its own source.

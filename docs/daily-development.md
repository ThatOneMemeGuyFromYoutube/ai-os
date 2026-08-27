# Daily development guardrails

Each incremental change should preserve these invariants:

1. Target 32-bit x86.
2. Keep the CCP/BDOS/transient-program separation understandable.
3. Prefer small, testable modules over clever abstractions.
4. Never remove the ability to create or compile programs.
5. Document user-visible commands with examples.
6. Keep GUI functionality optional and layered above the core ABI.
7. Add tests or an emulator smoke test whenever practical.

The daily work should be incremental rather than rewriting the architecture.
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
## GitHub Actions runtime

GitHub-hosted runners began using Node 24 for JavaScript actions by default on June 16, 2026, and removed the Node 20 runtime on September 23, 2026. All repository workflows already set `FORCE_JAVASCRIPT_ACTIONS_TO_NODE24=true`; keep action versions Node 24-compatible and do not use `ACTIONS_ALLOW_USE_UNSECURE_NODE_VERSION` as a routine fallback. See the [GitHub Actions Node 20 deprecation and removal notice](https://github.blog/changelog/2025-09-19-deprecation-of-node-20-on-github-actions-runners/).

## Emulator smoke-test helper

The bounded QEMU boot targets call `scripts/qemu-smoke.sh` rather than embedding shell exit-status checks in Make recipes. In a Make recipe, `$?` is an automatic variable listing newer prerequisites; keeping shell status handling in a script avoids accidentally testing a prerequisite name instead of `timeout`'s exit code. The helper accepts exit 124 as the expected timeout and propagates other failures.

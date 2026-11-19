#!/usr/bin/env bash
# Treat the expected timeout as a successful bounded boot smoke test, but
# preserve genuine QEMU failures. Kept outside Make so shell status variables
# cannot collide with Make's automatic-variable syntax.
set +e
timeout "\${QEMU_TIMEOUT:-8}s" "\${QEMU:-qemu-system-i386}" "$@"
status=$?
if [ "$status" -eq 124 ]; then
    exit 0
fi
exit "$status"

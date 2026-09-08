#!/usr/bin/env bash
set -euo pipefail

OUT_DIR="${1:-dist}"
mkdir -p "$OUT_DIR"

make check
make iso

cp build/kernel.bin "$OUT_DIR/kernel.bin"
cp build/hello.com "$OUT_DIR/hello.com"
cp build/ai-os.iso "$OUT_DIR/ai-os.iso"

sha256sum "$OUT_DIR/kernel.bin" "$OUT_DIR/hello.com" "$OUT_DIR/ai-os.iso" > "$OUT_DIR/SHA256SUMS"

cat > "$OUT_DIR/RELEASE_NOTES.md" <<EOF
# AsterOS release artifacts

## Included artifacts

- \\`kernel.bin\\` — linked 32-bit i386 kernel image
- \\`hello.com\\` — TinyLang example program image
- \\`ai-os.iso\\` — bootable GRUB ISO
- \\`SHA256SUMS\\` — SHA-256 checksums for all artifacts

## Validation

- \\`make check\\`
- \\`make iso\\`
- \\`qemu-system-i386 -cdrom ai-os.iso -display none -serial stdio -no-reboot -no-shutdown\\`
EOF

echo "Packaged release artifacts in $OUT_DIR"

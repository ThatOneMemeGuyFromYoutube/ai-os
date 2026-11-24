#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT_DIR/build/firmware}"
SOURCE_DIR="${SOURCE_DIR:-$ROOT_DIR/build/firmware/seabios-src}"
SEABIOS_TAG="rel-1.17.0"
SEABIOS_URL="https://git.seabios.org/seabios.git"
ASTERBIOS_CC="${ASTERBIOS_CC:-gcc}"
ASTERBIOS_LD="${ASTERBIOS_LD:-ld}"
ASTERBIOS_PYTHON="${ASTERBIOS_PYTHON:-python3}"

mkdir -p "$OUTPUT_DIR"
if [ ! -d "$SOURCE_DIR/.git" ]; then
  rm -rf "$SOURCE_DIR"
  git clone --depth 1 --branch "$SEABIOS_TAG" "$SEABIOS_URL" "$SOURCE_DIR"
fi

PATCH_FILE="$ROOT_DIR/firmware/asterbios/compat-test-link-i386.patch"
if git -C "$SOURCE_DIR" apply --check "$PATCH_FILE"; then
  git -C "$SOURCE_DIR" apply "$PATCH_FILE"
elif git -C "$SOURCE_DIR" apply --reverse --check "$PATCH_FILE"; then
  echo "SeaBIOS i386 linker-probe patch is already applied"
else
  echo "error: cannot apply AsterBIOS compatibility patch to the pinned SeaBIOS source" >&2
  exit 1
fi

actual_tag="$(git -C "$SOURCE_DIR" describe --tags --exact-match HEAD 2>/dev/null || true)"
if [ "$actual_tag" != "$SEABIOS_TAG" ]; then
  echo "error: expected SeaBIOS $SEABIOS_TAG source, found ${actual_tag:-untagged checkout}" >&2
  exit 1
fi

make -C "$SOURCE_DIR" clean
make -C "$SOURCE_DIR" CC="$ASTERBIOS_CC" LD="$ASTERBIOS_LD" PYTHON="$ASTERBIOS_PYTHON" EXTRAVERSION="-AsterOS"
test -s "$SOURCE_DIR/out/bios.bin"
bios_size="$(wc -c < "$SOURCE_DIR/out/bios.bin")"
if [ "$bios_size" -ne 131072 ]; then
  echo "error: expected a 128 KiB v86/QEMU BIOS image, got $bios_size bytes" >&2
  exit 1
fi
cp "$SOURCE_DIR/out/bios.bin" "$OUTPUT_DIR/asterbios.bin"
git -C "$SOURCE_DIR" archive --format=tar.gz --prefix="seabios-$SEABIOS_TAG/" "$SEABIOS_TAG" \
  -o "$OUTPUT_DIR/asterbios-source-$SEABIOS_TAG.tar.gz"
cp "$SOURCE_DIR/COPYING" "$OUTPUT_DIR/SeaBIOS-COPYING"
cp "$SOURCE_DIR/COPYING.LESSER" "$OUTPUT_DIR/SeaBIOS-COPYING.LESSER"
cp "$ROOT_DIR/firmware/asterbios/compat-test-link-i386.patch" "$OUTPUT_DIR/asterbios-build.patch"
printf "SeaBIOS tag: %s\\nSeaBIOS commit: %s\\nAsterBIOS suffix: -AsterOS\\nBuild patch: compat-test-link-i386.patch\\n" "$SEABIOS_TAG" "$(git -C "$SOURCE_DIR" rev-parse HEAD)" > "$OUTPUT_DIR/asterbios-build-info.txt"
printf 'Built AsterBIOS from SeaBIOS %s\n' "$SEABIOS_TAG"
printf 'ROM: %s\n' "$OUTPUT_DIR/asterbios.bin"
printf 'Corresponding source: %s\n' "$OUTPUT_DIR/asterbios-source-$SEABIOS_TAG.tar.gz"

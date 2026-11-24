#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT_DIR/build/firmware}"
SOURCE_DIR="${SOURCE_DIR:-$ROOT_DIR/build/firmware/seabios-src}"
SEABIOS_TAG="rel-1.17.0"
SEABIOS_URL="https://git.seabios.org/seabios.git"

mkdir -p "$OUTPUT_DIR"
if [ ! -d "$SOURCE_DIR/.git" ]; then
  rm -rf "$SOURCE_DIR"
  git clone --depth 1 --branch "$SEABIOS_TAG" "$SEABIOS_URL" "$SOURCE_DIR"
fi

actual_tag="$(git -C "$SOURCE_DIR" describe --tags --exact-match HEAD 2>/dev/null || true)"
if [ "$actual_tag" != "$SEABIOS_TAG" ]; then
  echo "error: expected SeaBIOS $SEABIOS_TAG source, found ${actual_tag:-untagged checkout}" >&2
  exit 1
fi

make -C "$SOURCE_DIR" clean
make -C "$SOURCE_DIR" EXTRAVERSION="-AsterOS"
test -s "$SOURCE_DIR/out/bios.bin"
cp "$SOURCE_DIR/out/bios.bin" "$OUTPUT_DIR/asterbios.bin"
git -C "$SOURCE_DIR" archive --format=tar.gz --prefix="seabios-$SEABIOS_TAG/" "$SEABIOS_TAG" \
  -o "$OUTPUT_DIR/asterbios-source-$SEABIOS_TAG.tar.gz"
cp "$SOURCE_DIR/COPYING" "$OUTPUT_DIR/SeaBIOS-COPYING"
cp "$SOURCE_DIR/COPYING.LESSER" "$OUTPUT_DIR/SeaBIOS-COPYING.LESSER"
printf "SeaBIOS tag: %s\\nSeaBIOS commit: %s\\nAsterBIOS suffix: -AsterOS\\n" "$SEABIOS_TAG" "$(git -C "$SOURCE_DIR" rev-parse HEAD)" > "$OUTPUT_DIR/asterbios-build-info.txt"
printf 'Built AsterBIOS from SeaBIOS %s\n' "$SEABIOS_TAG"
printf 'ROM: %s\n' "$OUTPUT_DIR/asterbios.bin"
printf 'Corresponding source: %s\n' "$OUTPUT_DIR/asterbios-source-$SEABIOS_TAG.tar.gz"

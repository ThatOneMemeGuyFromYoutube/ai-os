#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$ROOT_DIR/build/firmware}"
SOURCE_DIR="${SOURCE_DIR:-$ROOT_DIR/build/firmware/seabios-src}"
SEABIOS_TAG="rel-1.17.0"
SEABIOS_COMMIT="b52ca86e094d19b58e2304417787e96b940e39c6"
SEABIOS_URL="${SEABIOS_URL:-https://git.seabios.org/seabios.git}"
SEABIOS_MIRROR_URL="${SEABIOS_MIRROR_URL:-https://github.com/qemu/seabios.git}"
ASTERBIOS_CC="${ASTERBIOS_CC:-gcc}"
ASTERBIOS_LD="${ASTERBIOS_LD:-ld}"
ASTERBIOS_PYTHON="${ASTERBIOS_PYTHON:-python3}"
source_url="existing checkout"

mkdir -p "$OUTPUT_DIR"
if [ ! -d "$SOURCE_DIR/.git" ]; then
  rm -rf "$SOURCE_DIR"
  cloned=0
  for candidate_url in "$SEABIOS_URL" "$SEABIOS_MIRROR_URL"; do
    for attempt in 1 2; do
      echo "Cloning SeaBIOS $SEABIOS_TAG from $candidate_url (attempt $attempt/2)"
      if git clone --depth 1 --branch "$SEABIOS_TAG" \
          "$candidate_url" "$SOURCE_DIR"; then
        source_url="$candidate_url"
        cloned=1
        break 2
      fi
      rm -rf "$SOURCE_DIR"
      if [ "$attempt" -lt 2 ]; then sleep "$attempt"; fi
    done
  done
  if [ "$cloned" -ne 1 ]; then
    echo "error: unable to fetch the pinned SeaBIOS source from either upstream URL" >&2
    exit 1
  fi
fi

actual_tag="$(git -C "$SOURCE_DIR" describe --tags --exact-match HEAD 2>/dev/null || true)"
actual_commit="$(git -C "$SOURCE_DIR" rev-parse HEAD)"
if [ "$actual_tag" != "$SEABIOS_TAG" ] || [ "$actual_commit" != "$SEABIOS_COMMIT" ]; then
  echo "error: expected SeaBIOS $SEABIOS_TAG at $SEABIOS_COMMIT, found ${actual_tag:-untagged checkout} at $actual_commit" >&2
  exit 1
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

make -C "$SOURCE_DIR" clean
make -C "$SOURCE_DIR" CC="$ASTERBIOS_CC" LD="$ASTERBIOS_LD" PYTHON="$ASTERBIOS_PYTHON" EXTRAVERSION="-AsterOS"
test -s "$SOURCE_DIR/out/bios.bin"
bios_size="$(wc -c < "$SOURCE_DIR/out/bios.bin")"
if [ "$bios_size" -ne 131072 ]; then
  echo "error: expected a 128 KiB v86/QEMU BIOS image, got $bios_size bytes" >&2
  exit 1
fi
cp "$SOURCE_DIR/out/bios.bin" "$OUTPUT_DIR/asterbios.bin"
git -C "$SOURCE_DIR" archive --format=tar.gz --prefix="seabios-$SEABIOS_TAG/" "$SEABIOS_COMMIT" \
  -o "$OUTPUT_DIR/asterbios-source-$SEABIOS_TAG.tar.gz"
cp "$SOURCE_DIR/COPYING" "$OUTPUT_DIR/SeaBIOS-COPYING"
cp "$SOURCE_DIR/COPYING.LESSER" "$OUTPUT_DIR/SeaBIOS-COPYING.LESSER"
cp "$PATCH_FILE" "$OUTPUT_DIR/asterbios-build.patch"
printf "SeaBIOS tag: %s\nSeaBIOS commit: %s\nSeaBIOS source URL: %s\nAsterBIOS suffix: -AsterOS\nBuild patch: compat-test-link-i386.patch\n" "$SEABIOS_TAG" "$actual_commit" "$source_url" > "$OUTPUT_DIR/asterbios-build-info.txt"
printf 'Built AsterBIOS from SeaBIOS %s (%s)\n' "$SEABIOS_TAG" "$actual_commit"
printf 'ROM: %s\n' "$OUTPUT_DIR/asterbios.bin"
printf 'Corresponding source: %s\n' "$OUTPUT_DIR/asterbios-source-$SEABIOS_TAG.tar.gz"

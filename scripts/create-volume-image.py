#!/usr/bin/env python3
"""Create a disposable, empty AsterOS volume image for emulator validation.

This creates only the MBR partition entry and read-only volume descriptor.
It does not create a filesystem or imply that the guest can persist files.
"""

import argparse
from pathlib import Path
import struct
import zlib

SECTOR_SIZE = 512
PARTITION_TYPE = 0x7F
PARTITION_START = 2048
VOLUME_MAGIC = b"ASTEROS1"
VOLUME_HEADER_SIZE = 36


def build_image(output: Path, size_mib: int) -> None:
    if size_mib < 2:
        raise ValueError("image must be at least 2 MiB")
    total_sectors = size_mib * 1024 * 1024 // SECTOR_SIZE
    partition_sectors = total_sectors - PARTITION_START
    if partition_sectors < 2:
        raise ValueError("image is too small for the reserved AsterOS volume")

    mbr = bytearray(SECTOR_SIZE)
    entry = memoryview(mbr)[446:462]
    entry[0] = 0
    entry[4] = PARTITION_TYPE
    struct.pack_into("<II", entry, 8, PARTITION_START, partition_sectors)
    mbr[510:512] = b"\x55\xAA"

    header = bytearray(SECTOR_SIZE)
    header[:8] = VOLUME_MAGIC
    struct.pack_into(
        "<HHHHIIII",
        header,
        8,
        1,
        VOLUME_HEADER_SIZE,
        SECTOR_SIZE,
        0,
        PARTITION_START,
        partition_sectors,
        1,
        0,
    )
    struct.pack_into("<I", header, 32, zlib.crc32(header[:32]) & 0xFFFFFFFF)

    with output.open("xb") as image:
        image.truncate(total_sectors * SECTOR_SIZE)
        image.seek(0)
        image.write(mbr)
        image.seek(PARTITION_START * SECTOR_SIZE)
        image.write(header)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--size-mib", type=int, default=16)
    args = parser.parse_args()
    try:
        build_image(args.output, args.size_mib)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"created {args.output} with one read-only AsterOS volume descriptor")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check the release NRO's asset header and application metadata."""

import argparse
from pathlib import Path
import struct


def check(data):
    if len(data) < 0x80 or data[0x10:0x14] != b"NRO0":
        raise ValueError("Missing NRO header")
    asset = struct.unpack_from("<I", data, 0x18)[0]
    if asset < 0x80 or asset + 0x38 > len(data) or data[asset:asset + 8] != b"ASET\0\0\0\0":
        raise ValueError("Missing version 0 asset header")
    icon_offset, icon_size, nacp_offset, nacp_size, romfs_offset, romfs_size = (
        struct.unpack_from("<6Q", data, asset + 8)
    )
    if icon_offset or icon_size or romfs_offset or romfs_size:
        raise ValueError("Shell NRO contains unexpected icon or RomFS assets")
    if nacp_offset != 0x38 or nacp_size != 0x4000 or asset + nacp_offset + nacp_size != len(data):
        raise ValueError("Invalid NACP asset bounds")
    start = asset + nacp_offset
    title = data[start:start + 0x200].split(b"\0", 1)[0]
    author = data[start + 0x200:start + 0x300].split(b"\0", 1)[0]
    version = data[start + 0x3060:start + 0x3070].split(b"\0", 1)[0]
    if (title, author, version) != (b"EXODUS Shell", b"EXODUSx1", b"0.10.0"):
        raise ValueError("Unexpected shell NACP metadata")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("nro", type=Path)
    args = parser.parse_args()
    try:
        check(args.nro.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f"Shell release NRO check failed: {error}\n")
    print("Shell release NRO has the expected metadata and no embedded icon or RomFS.")


if __name__ == "__main__":
    main()

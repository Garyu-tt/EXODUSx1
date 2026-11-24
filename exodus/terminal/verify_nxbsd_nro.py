"""Structural validation only, not a device execution test."""
import hashlib
import json
from pathlib import Path
import struct
import sys

def verify(data):
    if len(data) < 0x80 or data[0x10:0x14] != b"NRO0":
        raise ValueError("missing NRO header")
    size = struct.unpack_from("<I", data, 0x18)[0]
    if not 0x80 <= size <= len(data):
        raise ValueError("invalid image size")
    segments = {}
    previous = 0
    for name, pos in (("text", 0x20), ("ro", 0x28), ("data", 0x30)):
        offset, length = struct.unpack_from("<II", data, pos)
        if offset < previous or not length or offset + length > size or offset % 0x1000:
            raise ValueError("invalid " + name + " segment")
        segments[name] = [offset, length]
        previous = offset + length
    if size + 0x38 > len(data) or data[size:size+4] != b"ASET":
        raise ValueError("missing asset metadata")
    for name, pos in (("icon", 8), ("nacp", 24)):
        offset, length = struct.unpack_from("<QQ", data, size + pos)
        if offset < 0x38 or not length or size + offset + length > len(data):
            raise ValueError("invalid " + name + " asset")
        if name == "nacp" and length != 0x4000:
            raise ValueError("invalid NACP length")
    return {"size": len(data), "image_size": size, "segments": segments,
            "sha256": hashlib.sha256(data).hexdigest(), "runtime": "not_tested"}

if __name__ == "__main__":
    print(json.dumps(verify(Path(sys.argv[1]).read_bytes()), indent=2))

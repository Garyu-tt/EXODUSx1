"""Strict QEMU reference ELF loader for locally built FreeBSD ARMv7 kernels.

Expected digest must come from the selected source build's inventory.
No hardware addresses or device writes; all output is disposable.
"""
import argparse
import hashlib
import pathlib
import re
import struct

LINK = 0xc0000000
RAM = 0x60000000
LOAD = 0x61000000
EH = struct.Struct("<16sHHIIIIIHHHHHH")
PH = struct.Struct("<8I")


def require(condition, reason):
    if not condition:
        raise ValueError(reason)


def plan(data, digest, ram_mib):
    require(re.fullmatch(r"[0-9a-f]{64}", digest), "invalid expected digest")
    require(hashlib.sha256(data).hexdigest() == digest, "kernel digest mismatch")
    require(24 <= ram_mib <= 1024, "unsupported reference RAM budget")
    require(len(data) >= EH.size, "truncated ELF header")
    ident, typ, machine, ver, entry, phoff, _, _, ehsize, phsize, phnum, *_ = EH.unpack_from(data)
    require(ident[:7] == b"\x7fELF\x01\x01\x01", "not little-endian ELF32")
    require((typ, machine, ver, ehsize, phsize) == (2, 40, 1, EH.size, PH.size), "not ARM executable")
    require(0 < phnum <= 128 and EH.size <= phoff and phoff + phnum * phsize <= len(data), "invalid program headers")
    require(entry % 4 == 0, "entry is not aligned ARM state")
    segments = []
    for i in range(phnum):
        t, off, va, pa, fs, ms, flags, align = PH.unpack_from(data, phoff + i * phsize)
        if t != 1:
            continue
        require(pa == va and LINK <= va and 0 < ms and fs <= ms and off + fs <= len(data), "invalid load segment")
        require(flags in (4, 5, 6), "unsupported segment permissions")
        require(align > 0 and not align & (align - 1) and off % align == va % align, "invalid segment alignment")
        dest = LOAD + va - LINK
        require(dest + ms <= RAM + ram_mib * 1024 * 1024, "load segment exceeds reference RAM")
        require(all(dest + ms <= d or d + m <= dest for d, m, *_ in segments), "overlapping load segments")
        segments.append((dest, ms, off, fs, flags))
    require(segments and min(d for d, *_ in segments) == LOAD, "unsupported linked base")
    physical = LOAD + entry - LINK
    require(any(d <= physical and physical + 4 <= d + fs and f & 1 for d, m, o, fs, f in segments), "entry outside file-backed executable segment")
    size = max(d + m for d, m, *_ in segments) - LOAD
    require(size <= 64 * 1024 * 1024, "reference image exceeds bootstrap window")
    image = bytearray(size)
    for d, m, o, fs, f in segments:
        image[d - LOAD:d - LOAD + fs] = data[o:o + fs]
    return physical, image, segments


def main():
    p = argparse.ArgumentParser()
    p.add_argument("kernel", type=pathlib.Path)
    p.add_argument("output", type=pathlib.Path)
    p.add_argument("--sha256", required=True)
    p.add_argument("--ram-mib", type=int, required=True)
    a = p.parse_args()
    entry, image, segments = plan(a.kernel.read_bytes(), a.sha256, a.ram_mib)
    a.output.mkdir(parents=True, exist_ok=True)
    (a.output / "kernel.bin").write_bytes(image)
    (a.output / "entry.txt").write_text(f"{entry:#010x}\n")
    for d, m, o, fs, f in segments:
        print(f"PT_LOAD {d:#010x} file={fs:#x} mem={m:#x} flags={f}")
    print(f"Source kernel entry {entry:#010x}, image bytes {len(image)}")


if __name__ == "__main__":
    main()

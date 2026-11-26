"""QEMU-only direct boot bundle for the pinned FreeBSD 15.1 ARM64 kernel."""
import hashlib
import struct
import sys
from pathlib import Path

KERNEL_SHA = "e66c772fb7b1144c61735c18dceb1f598f87f18b5acd9781c1d78327a6f68379"
VBASE = 0xffff000000000000
KERNEL_OFFSET = 0x200000
FDT_OFFSET = 0x2000000

def require(condition, message):
    if not condition:
        raise ValueError(message)

def bundle(kernel, boot, dtb):
    require(hashlib.sha256(kernel).hexdigest() == KERNEL_SHA, "unexpected kernel revision")
    require(kernel[:6] == b"\x7fELF\x02\x01", "not little-endian ELF64")
    require(struct.unpack_from("<H", kernel, 18)[0] == 183, "not AArch64")
    require(struct.unpack_from("<Q", kernel, 24)[0] == VBASE + 0x800, "unexpected entry")
    phoff = struct.unpack_from("<Q", kernel, 32)[0]
    phents, phnum = struct.unpack_from("<HH", kernel, 54)
    require(phents == 56 and phoff + phents * phnum <= len(kernel), "invalid ELF headers")
    require(0 < len(boot) < KERNEL_OFFSET, "invalid bootstrap size")
    require(40 <= len(dtb) <= 0x200000 and dtb[:4] == b"\xd0\x0d\xfe\xed"
            and struct.unpack_from(">I", dtb, 4)[0] == len(dtb), "invalid FDT")
    blob = bytearray(FDT_OFFSET)
    blob[:len(boot)] = boot
    regions = []
    for i in range(phnum):
        typ, flags, off, va, pa, fs, ms, align = struct.unpack_from("<IIQQQQQQ", kernel, phoff + i * phents)
        if typ != 1:
            continue
        at = KERNEL_OFFSET + va - VBASE
        require(KERNEL_OFFSET <= at <= at + ms <= FDT_OFFSET
                and fs <= ms and off + fs <= len(kernel), "invalid kernel segment")
        require(all(at + ms <= lo or at >= hi for lo, hi in regions), "overlapping segments")
        regions.append((at, at + ms))
        blob[at:at + fs] = kernel[off:off + fs]
    require(bool(regions), "no kernel segments")
    blob.extend(dtb)
    return blob

if __name__ == "__main__":
    if len(sys.argv) != 5:
        raise SystemExit("usage: pack_bao_freebsd.py kernel boot.bin guest.dtb guest-B.bin")
    kernel, boot, dtb, output = map(Path, sys.argv[1:])
    result = bundle(kernel.read_bytes(), boot.read_bytes(), dtb.read_bytes())
    output.write_bytes(result)
    print("QEMU-only FreeBSD boot bundle bytes:", len(result))

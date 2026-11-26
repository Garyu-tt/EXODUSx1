"""Experimental QEMU-only FreeBSD native loader metadata and RAM-disk bundle.

Use with bao_freebsd_boot.S compiled with TUMOR_FREEBSD_NATIVE_METADATA.
No physical target, no kernel modification, no userspace acceptance implied.
"""
import struct
import sys
import unittest
from pathlib import Path

from pack_bao_freebsd import bundle, require, VBASE, KERNEL_OFFSET, FDT_OFFSET

POINTER_OFFSET = 0x2200000
ENV_OFFSET = POINTER_OFFSET + 0x1000
ROOT_OFFSET = 0x2400000
MAX_ROOT = 64 * 1024 * 1024
GUEST_SIZE = 0x20000000


def align(n, a=4096):
    return (n + a - 1) & ~(a - 1)


def va(offset):
    return VBASE + offset - KERNEL_OFFSET


def record(kind, data):
    return struct.pack("<II", kind, len(data)) + data + bytes((-len(data)) % 8)


def number(kind, n):
    return record(kind, struct.pack("<Q", n))


def string(kind, value):
    return record(kind, value.encode("ascii") + b"\0")


def metadata(kernel_size, root_size, meta_offset):
    def build(end):
        return b"".join((
            string(1, "kernel"), string(2, "elf kernel"),
            number(3, VBASE), number(4, kernel_size),
            record(0x8007, struct.pack("<I", 0)),
            number(0x8006, va(ENV_OFFSET)),
            number(0x8008, va(end)),
            number(0x9002, va(FDT_OFFSET)),
            string(1, "mfsroot"), string(2, "md_image"),
            number(3, va(ROOT_OFFSET)), number(4, root_size),
            record(0, b""),
        ))
    result = build(0)
    end = align(meta_offset + len(result))
    require(end + 0x600000 <= GUEST_SIZE, "metadata exceeds guest RAM")
    return build(end)


def root_bundle(kernel, boot, dtb, root):
    require(0 < len(root) <= MAX_ROOT and len(root) % 512 == 0,
            "root image must be sector-aligned and at most 64 MiB")
    image = bundle(kernel, boot, dtb)
    require(len(image) <= POINTER_OFFSET, "FDT overlaps metadata pointer")
    phoff = struct.unpack_from("<Q", kernel, 32)[0]
    phents, phnum = struct.unpack_from("<HH", kernel, 54)
    ends = []
    for i in range(phnum):
        typ, _, _, addr, _, _, memsz, _ = struct.unpack_from(
            "<IIQQQQQQ", kernel, phoff + i * phents)
        if typ == 1:
            ends.append(addr + memsz - VBASE)
    kernel_size = max(ends)
    meta_offset = align(ROOT_OFFSET + len(root))
    meta = metadata(kernel_size, len(root), meta_offset)
    image.extend(bytes(align(meta_offset + len(meta)) - len(image)))
    struct.pack_into("<Q", image, POINTER_OFFSET, va(meta_offset))
    env = b"vfs.root.mountfrom=ufs:/dev/md0\0\0"
    image[ENV_OFFSET:ENV_OFFSET + len(env)] = env
    image[ROOT_OFFSET:ROOT_OFFSET + len(root)] = root
    image[meta_offset:meta_offset + len(meta)] = meta
    return image


class MetadataTests(unittest.TestCase):
    def test_layout(self):
        root_size = 8192
        pos = align(ROOT_OFFSET + root_size)
        raw = metadata(0x100000, root_size, pos)
        records = []
        offset = 0
        while offset < len(raw):
            kind, size = struct.unpack_from("<II", raw, offset)
            start = offset + 8
            records.append((kind, raw[start:start + size]))
            self.assertEqual(raw[start + size:start + align(size, 8)],
                             bytes((-size) % 8))
            offset = start + align(size, 8)
        self.assertEqual(offset, len(raw))
        self.assertEqual(records[-1], (0, b""))
        self.assertEqual(records[1], (2, b"elf kernel\0"))
        self.assertEqual(records[9], (2, b"md_image\0"))
        self.assertEqual(records[10], (3, struct.pack("<Q", va(ROOT_OFFSET))))
        self.assertEqual(records[11], (4, struct.pack("<Q", root_size)))
        self.assertEqual(records[5], (0x8006, struct.pack("<Q", va(ENV_OFFSET))))
        self.assertEqual(records[6], (0x8008, struct.pack("<Q", va(align(pos + len(raw))))))
        self.assertEqual(records[7], (0x9002, struct.pack("<Q", va(FDT_OFFSET))))

    def test_guest_bound(self):
        with self.assertRaises(ValueError):
            metadata(4096, 512, GUEST_SIZE)

    def test_sector_bound(self):
        for bad in (b"", b"x", b"x" * 513):
            with self.assertRaisesRegex(ValueError, "sector-aligned"):
                root_bundle(b"", b"", b"", bad)


if __name__ == "__main__":
    if sys.argv[1:] == ["--selftest"]:
        unittest.main(argv=[sys.argv[0]])
    else:
        if len(sys.argv) != 6:
            raise SystemExit("usage: pack_bao_freebsd_root.py kernel boot.bin guest.dtb root.ufs guest-B.bin")
        kernel, boot, dtb, root, output = map(Path, sys.argv[1:])
        require(0 < root.stat().st_size <= MAX_ROOT, "root image exceeds bound")
        output.write_bytes(root_bundle(kernel.read_bytes(), boot.read_bytes(),
                                       dtb.read_bytes(), root.read_bytes()))

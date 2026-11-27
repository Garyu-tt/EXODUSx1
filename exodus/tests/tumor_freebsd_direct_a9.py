"""Offline relocation plan for the exact audited FreeBSD release kernel."""
import hashlib
import pathlib
import struct
import sys

kernel = pathlib.Path(sys.argv[1]).read_bytes()
out = pathlib.Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
assert hashlib.sha256(kernel).hexdigest() == "0c535d1cbe4640c676531b0a9fcca9eef704e008521d858541e72f9a88d2806d"
assert kernel[:7] == b"\x7fELF\x01\x01\x01"
h = struct.unpack_from("<16sHHIIIIIHHHHHH", kernel)
_, typ, machine, ver, entry, phoff, _, _, ehsize, phsize, phnum, *_ = h
assert (typ, machine, ver, ehsize, phsize, phnum) == (2,40,1,52,32,12)
assert phoff+phsize*phnum <= len(kernel)
link, load, limit = 0xc0000000, 0x61000000, 0x64000000
assert load % 0x100000 == 0
segments = []
for i in range(phnum):
    t, off, va, pa, fs, ms, flags, align = struct.unpack_from("<8I",kernel,phoff+i*phsize)
    if t != 1:
        continue
    assert pa == va and va >= link and 0 < ms and fs <= ms and off+fs <= len(kernel)
    assert flags in (4,5,6) and align and not align&(align-1)
    assert off%align == va%align
    dest = load + va-link
    assert dest+ms <= limit
    assert all(dest+ms <= d or d+m <= dest for d,m,*_ in segments)
    segments.append((dest,ms,off,fs,flags))
assert len(segments)==5
assert entry==0xc0000200
phys_entry=load+entry-link
assert any(d<=phys_entry and phys_entry+4<=d+fs and f&1 for d,m,o,fs,f in segments)
image=bytearray(max(d+m for d,m,*_ in segments)-load)
for d,m,o,fs,f in segments:
    image[d-load:d-load+fs]=kernel[o:o+fs]
    print(f"PT_LOAD {d:#010x} file={fs:#x} mem={m:#x} flags={f}")
(out/"kernel.bin").write_bytes(image)
print(f"Direct reference entry {phys_entry:#010x}, image bytes {len(image)}")

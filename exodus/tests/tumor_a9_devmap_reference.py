"""QEMU-only guarded patch of audited FreeBSD gem5 UART mapping.
Never install this experimental ELF on physical hardware.
"""
import hashlib, pathlib, struct, subprocess, sys
src=pathlib.Path(sys.argv[1])
dst=pathlib.Path(sys.argv[2])
data=bytearray(src.read_bytes())
assert hashlib.sha256(data).hexdigest()=="0c535d1cbe4640c676531b0a9fcca9eef704e008521d858541e72f9a88d2806d"
lines=subprocess.check_output(["arm-linux-gnueabihf-nm","-S",str(src)],text=True).splitlines()
matches=[s.split() for s in lines if s.split()[-1:]==["gem5_devmap_init"]]
assert len(matches)==1 and len(matches[0])==4, matches
addr,size=int(matches[0][0],16),int(matches[0][1],16)
assert 4<=size<=256 and size%4==0
h=struct.unpack_from("<16sHHIIIIIHHHHHH",data)
off,ents,n=h[5],h[9],h[10]
segments=[struct.unpack_from("<8I",data,off+i*ents) for i in range(n)]
mapping=[(o+addr-va) for t,o,va,pa,fs,ms,flags,align in segments if t==1 and flags&1 and va<=addr and addr+size<=va+fs]
assert len(mapping)==1
start=mapping[0]
# MOVT r0,#0x1c09 -> MOVT r0,#0x1000 (base of actual UART's 1 MiB window).
# Refuse any other compiler encoding; never silently patch an unknown instruction.
old,new=0xe3410c09,0xe3410000
hits=[i for i in range(start,start+size,4) if struct.unpack_from("<I",data,i)[0]==old]
assert len(hits)==1, "expected unique MOVT r0,#0x1c09 in gem5_devmap_init"
struct.pack_into("<I",data,hits[0],new)
dst.write_bytes(data)
print(f"QEMU-only devmap patch at {addr+hits[0]-start:#x}: {old:#010x} -> {new:#010x}")
print("Patched ELF SHA256:",hashlib.sha256(data).hexdigest())

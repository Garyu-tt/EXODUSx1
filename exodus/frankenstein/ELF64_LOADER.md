# R13: bounded ARM64 ELF loader (first component)
Independent of the pending R12 runtime gate. Strict little-endian ELF64
ET_EXEC / EM_AARCH64 parser, page-aligned 4 KiB PT_LOAD, 16 segment limit,
bounded program-header table, non-overlapping rounded mappings, RX/R/RW
only (W^X), executable file-backed aligned entry, no interpreter or dynamic
linking. Validate the complete plan before writing guest backing. Zero the
whole exclusive window, copy file-backed bytes, zero BSS and gaps. On
validation failure leave backing and plan unchanged. Caller owns immutable
source, disjoint storage and page-table installation; no raw guest pointers.

CI builds a genuine AArch64 ELF using GNU as/ld, checks the actual loaded
bytes and BSS under ASan/UBSan, malformed headers, integer overflow, overlap,
entry and permissions, and separately runs the original fixture under
qemu-aarch64 as a sanity check. That separate run is NOT execution of the
loaded backing under EXODUS. Next gate: install the plan into the R12 EL0
address space and execute the loaded image, then adapt real FreeBSD ABI.
No PIE, ET_DYN, TLS, relocations, dynamic libraries or physical deployment.
Refs #103 #105 #365.

# Tumor A57 inventory, R01 component

Board Probe R02 identified Mariko/Hoag and configured 4096 MiB on the
owner's console; SD report SHA-256
db43ccf74569ec93102edd277d8241ffa1ddb173e0f4a939adca19ea4f162a3c.
That was BPMP execution only.

This AArch64 callable leaf routine reads five architected registers and
writes a caller-owned 48-byte aligned little-endian record:

| Offset | uint64 field |
| --- | --- |
| 0 | CurrentEL |
| 8 | MIDR_EL1 |
| 16 | MPIDR_EL1 |
| 24 | ID_AA64PFR0_EL1 |
| 32 | ID_AA64MMFR0_EL1 |
| 40 | completion marker 0x4157, written last |

Caller must own and zero the buffer, establish valid execution context
and trap handling, and ensure visibility to any other processor. The
barriers do not establish coherence. A complete marker is not provenance.
CurrentEL is context-specific; the EL2 feature field alone does not prove
entry or runtime coexistence. No EL transitions, device register writes,
firmware or bootloader changes. No ADSP execution claim.

The donor hwtest CPU stub includes PCIe/WLAN operations and board power
sequencing. Those are intentionally not included here. This is an object
component, not a chainloadable payload. Physical caller, verified memory
ownership, timeout and recovery remain separate admission work.

Locally cross-assembled with clang and inspected with llvm-objdump.
No physical execution or runtime tests yet.

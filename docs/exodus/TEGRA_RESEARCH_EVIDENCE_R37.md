# R37 — EFI data abort diagnosis and normal-memory mapping

R36 run 46033302012, job 140622291195, failed after successfully building patched U-Boot and verifying the FreeBSD image. The same LDRH instruction and EFI image offset 0x46c90 faulted. Clearing SCTLR.A at initialization did not fix the observed failure. Exception artifact 15214933191 exists but has not been independently retrieved.

Pinned U-Boot arch/arm/lib/cache-cp15.c blob 947012f29963d0663b02b77c348a17098df6fd77 shows that cache enable can establish MMU mappings. cmd/cache.c implements dcache on. MMU-disabled data accesses may have unsuitable memory attributes for unaligned access; this is a hypothesis, not a confirmed R36 fault classification.

R37 commit a565710066c416730872e18a587e2200b21e991c retains initial SCTLR.A change, enables reference-board data cache/MMU before bootefi, requires cache status ON, and instruments EFI entry SCTLR and U-Boot data-abort SCTLR/DFSR/DFAR. Captures QEMU exception trace, source diff and boot log. This modifies only disposable QEMU U-Boot, not deployed hardware. CI https://github.com/Garyu-tt/EXODUSx1/actions/runs/46033712413 (in_progress). Diagnostic changes are combined with the mapping intervention; they do not independently prove which memory attribute caused R36. Real FreeBSD on Cortex-A9 remains unconfirmed. No console access.

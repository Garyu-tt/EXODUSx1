# R65 — cross-guest RAM exchange observed, CI evidence correction

R64 run 46053603088 both jobs failed final CI gate despite functional success: guest markers EXODUS_GUEST_RAM_PASS and EXODUS_GUEST_RAM_EXIT_OK present, ARM64 check process returned EXODUS_ARM64_RAM_PASS on same line as guest shell prompt, so anchored grep failed. At 32 MiB hw.realmem=33550336 exactly (32 MiB minus reserved page), hw.physmem=26976256. Host AArch64 initialized payload and checked guest response in shared file-backed RAM. Guest binary verified before execution. This is serialized request/reply in QEMU, not concurrent hardware cache-coherence proof.

R65 commit 4c28aaddc822e170f27eb82588b86e4d4f69bbc1 separates exact host command results into arm64-peer.log, retains exact guest marker checks, adds realmem validation and independent full 4096-byte backing-page comparison after emulator stop, including zeros outside expected fields. Same kernel, guest and peer programs. No weakening of success criteria. R65 runtime not yet confirmed. No physical device access; no production driver or second full guest kernel.

R64: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46053603088
R65: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46053936814

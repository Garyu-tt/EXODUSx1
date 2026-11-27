# R36 — A9 EFI alignment fault isolation

R34 run 46032548927 / job 140619725879 passed. Real FreeBSD 15.1 ARM ELF32 kernel entry 0xc0000200, 12 program headers, five PT_LOAD segments with p_paddr equal to p_vaddr. Prior synthetic ELF loader must not assume these are physical ADSP placement addresses.

R35 run 46032802094 / job 140620579018 failed after U-Boot initialized SD and read 836460-byte BOOTARM.EFI. EFI code fault at image offset 0x46c90, opcode e1d020b2 (LDRH r2,[r0,#2]), r0=0x7eb1c0b5 (odd halfword address). U-Boot pinned arch/arm/cpu/armv7/start.S blob b63481b43ca6891f828c7fdea14d6958cbae1fe7 explicitly sets SCTLR.A. Strong alignment-abort hypothesis; DFSR not captured in R35, so not proven.

R36 workflow commit cda6685b7983517edb238b08b1f017609702edc1 changes only initial alignment-check bit in disposable U-Boot experiment and adds QEMU exception trace. Records source diff. CI https://github.com/Garyu-tt/EXODUSx1/actions/runs/46033302012 in progress. No physical hardware or production bootloader modification. A successful EFI step would not establish full kernel boot, nor actual Tegra ADSP support.

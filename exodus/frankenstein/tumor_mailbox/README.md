# R34 reference resource binding

The optional FDT fragment describes exactly the existing Bao reference shared window, 64 KiB at guest IPA 0xf0000000. It is disabled by default and not included in the accepted R32 images. The driver accepts only the reference root model, compatible, explicit role and coherent-normal-memory assertion, and exact resource range. It reserves via the FreeBSD resource manager, avoids the default Device memory mapping and requests normal write-back memory. It deliberately performs no loads/stores of shared data and exposes no character device or arbitrary mapping. The FDT flag is a trusted mediator assertion, not a proof of stage-2 memory attributes. No live attach or hardware acceptance yet.

Build natively on matching FreeBSD ARM64 source with make in this directory (SYSDIR points to matching kernel sources). Next gate: build module, confirm the Bao stage-2 memory type and address mapping in both guests, then explicitly enable the fragment in a separate reference image and load it using a supported kernel module path. Only then connect the R33 mailbox codec; prove two directions and timeout. No /dev/mem, physical Tegra address reuse or boot payload.

## Source audit and compile gate

Pinned Bao 0af4a1ab558ad60c9658af4f43756c4536dd3141 uses normal inner/outer write-back flags for VM RAM, but PTE_VM_FLAGS specifies PTE_SH_NS (non-shareable). This alone does not establish cross-guest coherent atomics; do not enable the fragment until the effective stage-1/stage-2 attributes and actual mapping are verified. No change to Bao flags is implied. Dedicated native-build CI boots a disposable FreeBSD ARM64 VM, verifies official release src.txz against its MANIFEST, builds the module and checks AArch64 ELF64 ET_DYN module, without loading it. This is separate from the expensive graphics and dual-kernel matrices.

Initial native build job 139503559909 completed compilation and linking of the final AArch64 shared-object .ko, but the post-build REL assertion was incorrect (REL describes intermediate .kld). The revised gate requires DYN, ELF64, little endian, module metadata section and verified guest-to-host artifact export. Pin the actually observed official release source SHA256. Module remains unloaded; no runtime claim.

## Native build acceptance and loader control

Exact head 63713389b18e7cbb60a61bea40c24c14d2be3209 passed seven CI jobs. Native job 139515365030 (run 45702329404) produced artifact 15064386832, with guest build result 0 and independent host checksum/ELF gate PASS. Next control loads and unloads the same module twice on ordinary disposable FreeBSD ARM64 QEMU virt with no reference mailbox node. Require kernel module registration, absence of attached devices and removal of registration. This does not enable the FDT fragment, touch shared memory, or establish Bao coherency. The stage-2 gate remains independent.

## R36 mapping-only control

R34 head c96b4936de00ff904ef1a77cb0c5c3800d352096 passed native load/unload twice on stock QEMU FreeBSD (job 139521641776, artifact 15064991707). No device attached. R36 separates non-cacheable mapping-only attachment from coherent write-back mode. Exactly one zero-length FDT mode flag is required. The driver still never reads or writes mapped memory. The FDT preparation script admits only the two fixed Bao reference DTS files and roles; it creates a separate enabled node with mapping-only, never coherent-normal-memory. The original boot DTS files and disabled fragment remain unchanged. Host FDT compilation and inspection and native module build/load control are required. Actual mapping attachment under Bao remains a subsequent runtime gate; this change does not establish stage-2 coherency or permit data exchange.

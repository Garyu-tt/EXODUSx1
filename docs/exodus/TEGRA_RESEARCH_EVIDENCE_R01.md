# Tegra research evidence R01

Date: 2026-11-27. Scope: existing Tumor architecture (#103, #105, #349). Documentation only; no new product architecture, device access, deployment, or physical acceptance. Public source evidence is not equivalent to verification on the owner's device.

## Source ledger

| ID | Public source | Observed evidence | Applicability / unresolved |
| --- | --- | --- | --- |
| T01 | [FreeBSD e903478919602c90fdc202a8628b89eb7c3bc104](https://cgit.FreeBSD.org/src/commit/?id=e903478919602c90fdc202a8628b89eb7c3bc104) ([commit notice](https://lists.freebsd.org/pipermail/dev-commits-src-main/2020-December/000169.html)) | TX1/Nano port: 18 files, 10,773 additions; MAX77620, CAR, PLL, PMC, pinmux, cpufreq, USB padctl | Historical SoC driver donors, not proven Switch board drivers or safe concurrent ownership. Audit current FreeBSD and license notices. |
| T02 | [ChromiumOS Tegra210 ADSP audio driver](https://chromium.googlesource.com/chromiumos/third_party/kernel/+/firmware-strago-7287.B-chromeos-3.18/sound/soc/tegra-alt/tegra210_adsp_alt.c) | tegra210_adsp_init calls nvadsp_os_load/start, nvadsp_app_load and nvadsp_os_suspend | Independent OS/app lifecycle in NVIDIA BSP; not a supported Horizon extension API. Full underlying loader not reviewed. |
| T03 | [Linux Tegra SoC Kconfig](https://kernel.googlesource.com/pub/scm/linux/kernel/git/arm64/linux/+/06898e5320b5c12dd8c21d0ee8d5a68347001746/drivers/soc/tegra/Kconfig) and [34C3 Switch architecture](https://switchbrew.github.io/34c3-slides/) | ADSP Cortex-A9 / ARMv7 | ARM64 FreeBSD image cannot execute on this CPU. ARMv7 board port or a minimal supervisor plus AArch64 kernel remain distinct hypotheses. |
| T04 | [Linux Tegra210 MC source, blob f58f3ef6f68191ede7098bfa4dad546ce4de275d as observed](https://linux.googlesource.com/linux/kernel/git/bpf/bpf-next/+/refs/heads/master/drivers/memory/tegra/tegra210.c) | MC clients, SMMU groups, 128 ASIDs, 48 TLB lines, display grouping, 34-bit MC addresses | SoC driver declarations; no claim that all groups are accessible, independent or free under Horizon. CPU stage-2 does not by itself isolate device DMA. |
| T05 | [Switch board DTS reference mirror](https://notdouble.com/p/alarm-tegra-x1/file/reference/switchroot-mainline/tegra210-nintendo-switch.dts) | 4 GiB memory declaration and MAX77620 node; reserved-memory includes ramoops | Secondary source, confirm upstream revision and board variant before use. Not Horizon live allocation map. |
| T06 | [Switchroot Linux features](https://wiki.switchroot.org/wiki/linux/linux-features) | Erista/Mariko and multiple board variants supported; separate nvpmodel configuration | Alternative boot evidence only; distinguish revision and board. |
| T07 | [Switchbrew audio services](https://switchbrew.org/wiki/Audio_services) | Search index describes audio work memory and ADSP firmware changes | Direct page fetch returned HTTP 403 during this review; treat as pending verification, not a confirmed protocol contract. |

## Admission work (host first)

- A57/ADSP: verify ISA, ABI, word widths and address representability; shared records use explicit fixed-width serialization rather than native pointers. Validate malformed records, queue wrap, restart epochs and timeout behavior in existing fixtures.
- DMA/MMU: catalog clients and isolation groups from T04 and board sources; model exclusive owner, quiesce/fence, map/unmap, rollback, and reject unmapped access. CPU-only emulation cannot prove hardware coherency.
- Power/clocks: inventory FreeBSD T01 driver dependencies against board DTS; suppress unreviewed reset/clock/power writes in coexistence design. Verify resource ownership before enabling driver attach.
- Audio: document existing owner and resource lifecycle. Do not reset or replace active audio firmware based on T02 alone; keep audio continuity a physical gate.
- Physical test queue: only unanswered questions after source comparison, compile and host test. Batch board/revision confirmation, concurrent liveness, shared memory and cache/DMA, audio continuity, ownership handoff and recovery. Require rollback and separately recorded physical evidence.

## Status

Source inspection performed, not compilation or emulation. No hardware access. Source revisions and licenses need a complete audit before code reuse. No claim that stock Horizon permits the proposed ADSP or EL2 arrangement. Cross-reference existing #349 admission graph instead of creating a second graph.

## R02 — NVADSP host stack (2026-11-27)

Directly inspected ten files (4,509 lines including trailing newline counts) in [CTCaer Switch L4T](https://github.com/CTCaer/switch-l4t-kernel-nvidia/tree/linux-dev/drivers/platform/tegra/nvadsp). A second directory inventory in [Jetson TX1 L4T r27.1](https://github.com/Jetson-TX1-AndroidTV/android_kernel_jetson_tx1_linux-4.4/tree/l4t/l4t-r27.1/drivers/platform/tegra/nvadsp) corroborates the presence of the stack, but is not independent hardware evidence. Blob hashes identify the reviewed versions.

| File | Blob | Evidence / implication |
| --- | --- | --- |
| os.c | b85221e857b0c55eaf02107684c61a02f64ef29d | Host firmware request, ELF loader, mapping and coherent DMA allocation, app heap setup, diagnostics and OS lifecycle. Firmware is separate, not supplied by this source. |
| app_loader_linker.c | 3e64bdd99a51657ec0e17b116f4a77291fbaf90e | ARM32 ELF validation and dynamic module layout/linking. Not a general-purpose FreeBSD executable loader. |
| adsp_shared_struct.h | 5859a04d4e99f8069c983251c336f4ec8f8a0386 | Packed fixed-width host/ADSP records, two 4096-byte queue unions, separate app memory sizes, OS info and exception records. This is L4T ABI, not Horizon ABI. |
| msgq.c | 79ea96a9d99369658bd4d89eefcbcc364aba3b6c | Word-based circular queue, wrap handling and ENOSPC when capacity is exhausted; no inference of cross-CPU ordering guarantees from this function alone. |
| mailbox.c | 3b197a6e1b5a5de5b83f82af94ffe109fdea3caa | Host queue, completion and spinlock operations, mailbox send/receive. Host locks alone cannot prove shared-memory coherence. |
| dram_app_mem_manager.c | 08384aa1049f8986882fe0c14170e2d2c84bfcf6 | App allocation/release/address lookup abstraction. |
| nvadsp_shared_sema.c | 6c591466959dcabb36d2822777ae9beb950e761a | Shared semaphore source; protocol requires further review. |
| adsp_lpthread.c | 8a827a298c42ab20ebc1363b066bba2ff9fc4fc8 | Host control of special ADSP workload thread; not evidence of the complete firmware scheduler. |
| adsp_cpustat.c | 033a8e503331a43b8c34317acc54dc76b6b993d6 | Activity monitor, current/max usage reporting and ordering barriers. |
| os-t21x.c | a445d5a068a59cddf8df2ac401d417385ec1cc97 | Tegra210-specific init stub; major lifecycle implementation in os.c. |

Switchbrew search index ([Audio services](https://switchbrew.org/wiki/Audio_services)) includes an S2-specific statement about AudioAdsp KIP/HOS. Do not apply that statement to Switch 1 firmware; direct page access still returns 403. The retrieved historical backupbrew Audio services file (blob 65d23c6bd50154471c81e5c9e20a5645689818e2) did not contain those exact phrases. Horizon audio protocol remains unresolved.

Host-only next acceptance: extend existing Tumor fixtures with bounded fixed-width wire records, full/empty/wrap and malformed-length cases, separate address/opaque-handle types, completion timeout and peer restart, explicit cache/DMA ownership abstractions. Compare actual fixture contracts before implementing; do not import L4T protocol blindly. No firmware binary acquired, no scheduler source established, no compilation, no console access. Source inspection does not authorize touching the live audio owner.

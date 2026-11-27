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

## R03 — APE local memory, IRQ, DMA and power lifecycle

Primary hardware reference: [NVIDIA Tegra X1 embedded datasheet DS-07224-007 v1.0, p.10](https://cdck-file-uploads-global.s3.dualstack.us-west-2.amazonaws.com/nvidia/original/2X/1/1eecf66463f700c4515bfe091b2a0f6527d13571.pdf), section 1.2.5. Cortex-A9 with NEON/FPU, 32 KiB instruction and 32 KiB data L1, 128 KiB L2, 64 KiB audio RAM, dedicated audio clocks. This specifies generic T210, not available resources under Horizon.

Reviewed [Switch L4T NVADSP directory](https://github.com/CTCaer/switch-l4t-kernel-nvidia/tree/linux-dev/drivers/platform/tegra/nvadsp) with these source blobs:

| File | Blob | Observation |
| --- | --- | --- |
| amc.h | 2bf7d699975eab0ffa62ccb580a26604f22b036e | 64 KiB ARAM, 2 KiB aperture, four aliases; alias count is not additional memory. |
| amc.c | 1d37d4f215f70a142fb25b0e067b071bc6254e18 | ARAM save/restore, AMC state and invalid-access IRQ handling. |
| aram_manager.c | 62182c7af7e5a69507956f22cd34fbd83f18bb8b | Allocator wrapper for caller-supplied range, not proof of free space. |
| mem_manager.c | f0dc582e118961673188290500e5ba0bcf429a1e | Host-side allocation/release implementation. |
| hwmailbox.c | 9ae54b8bdc48e48472ba9e12e9d08948253500db | Separate receive-full and send-empty IRQ handlers. |
| hwmailbox.h | b079296109f1d5c257bf890709adca323ffb5066 | 32-bit mailbox word; short form has validity/type plus 10-bit mailbox ID and 20-bit data; 1024-entry host software queue, not hardware FIFO depth. |
| dev-t21x.c | 6ae797e2d4a5d0c96b81fb17a10ab6c7afdbd716 | Runtime PM saves/restores ARAM and AMISC; clock ownership and state loss are mandatory design constraints. |
| nvadsp_shared_sema.c | 6c591466959dcabb36d2822777ae9beb950e761a | **Negative evidence:** init returns NULL; destroy/acquire/release return -ENOENT. Shared semaphore API is a stub in this snapshot. No hardware arbitration claim. |

Independent source: [upstream Linux Tegra210 DTS at 1447e43](https://code.googlesource.com/linux/torvalds/linux/+/1447e43c0b735ffc0d48a6aec4f74d6877caffc2/arch/arm64/boot/dts/nvidia/tegra210.dtsi), ACONNECT subtree. ADMA lists 22 per-channel interrupt entries (SPI 24–45) under AGIC; AGIC has parent SPI 102, and ACONNECT is attached to audio power domain with APE/APB2APE clocks. [Original binding discussion](https://lkml.rescloud.iu.edu/1510.0/02683.html) corroborates channel count. ADMA is audio-specific; do not assume arbitrary memory-to-memory transfers or free channels.

Host acceptance additions: model finite ARAM capacity and alias identity; reject overcommit; distinguish hardware notification from software queue; model lost/duplicate notification, full queue, suspend/resume generation changes and power loss. Do not assume hardware semaphores; require a proven cross-peer ordering and ownership protocol before enabling shared writers. Reserve audio IRQ/DMA resources by default until explicit allocation evidence. These are test requirements, not executed results. No console access, firmware modification, code import, or hardware validation.

## R04 — HOST1X, VIC, video codecs and graphics backend donors (2026-11-27)

| Source | Observed or reported | EXODUS implication / limit |
| --- | --- | --- |
| [NVIDIA DS-07224-007 v1.0, p.9](https://forums.developer.nvidia.com/uploads/short-url/4pA0RhQeOC4TEwqPuGNml7uV4Nb.pdf) | VIC supports 2D compositing, scale, rotate, format conversion, blit, color decompression and video filters. JPEG engine is separate. | Candidate 2D offload, not programmable GPU or generic compression. |
| [Linux host1x dev.c at c5ac744c](https://gbmc.googlesource.com/linux/+/c5ac744cdddae82916d4cd35d962d3f47065e68a/drivers/gpu/host1x/dev.c) | host1x05_info matched to Tegra210: 14 channels, 192 syncpoints, 16 mlocks, 64 bases, 34-bit DMA mask; has_hypervisor=false, has_wide_gather=false. | Linux driver capability declaration, not free-resource inventory under Horizon; do not infer CPU EL2 support from HOST1X flag. |
| [Linux VIC driver](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/tegra/vic.c) | T210 VIC firmware filename vic04_ucode.bin, host1x client. | Firmware dependency; driver source is not microcode source. |
| [Switchbrew NV services](https://switchbrew.org/wiki/NV_services) and [historical mirror](https://github.com/backupbrew/switchbrew/blob/master/NV%20services.md) | Horizon exposes separate VIC and NVDEC channels through NVIDIA service. | Use existing authorized service contracts; no assumption Linux ioctl ABI matches. |
| [Averne FFmpeg NVTEGRA patch series, 2024](https://ffmpeg.org/pipermail/ffmpeg-devel/2024-May/328553.html), [hardware context](https://ffmpeg.org/pipermail/ffmpeg-devel/2024-May/328555.html), [SwitchWave](https://github.com/averne/SwitchWave) | Public engine definitions and reported native hardware decoding. | Candidate decoder and buffer-contract donors; upstream merge status and licenses require checking. |
| [Switch Frame Tap engineering README](https://github.com/TomasUribe/switch-frame-tap/blob/master/README.md) | Authors report >180 physical test runs and document native VIC/NVENC use, clock dependencies, frame fences, format/layout mismatches and contention. Reports stalls of 50–200 ms under some workloads. | Valuable external test evidence; not independently reproduced, not a dual-kernel test. Treat uncertain completion as failure, preserve stock compositor and recording owner. |
| [Mesa Switch port](https://github.com/danfromtico/mesa-switch), [port architecture](https://github.com/danfromtico/mesa-switch/blob/main/docs/switch-opengl.rst) | Project advertises native NVK Vulkan, NVC0/Zink GL and EGL over a shared libnx backend, explicit cache policies and syncpoint fences. No DMA-BUF/FD sharing; EGL conformance incomplete. | Candidate native graphics backend for existing EXODUS port work, not a FreeBSD driver and not proof of game compatibility. |
| [libdrm test package](https://packages.debian.org/sid/arm64/libdrm-tests/filelist) | tegra-vic-blit and tegra-vic-clear shipped as tests. | Reuse test concepts; these require Tegra hardware to validate actual engine output. |
| [Switchroot bootstack](https://gitlab.com/switchroot/bootstack/documentation) and [Linux T210 BPMP binding](https://git.zx2c4.com/wireguard-linux/tree/Documentation/devicetree/bindings/firmware/nvidia%2Ctegra210-bpmp.txt?id=840e5bb326bbcb16ce82dd2416d2769de4839aea) | BPMP handles power/clock/suspend and DRAM training; binding describes CPU IPC, atomics and semaphore resources. Switchroot notes T210B01 BPMP source not public. | Critical service, not spare compute. BPMP synchronization is not proof of ADSP semaphore accessibility. |

Host-first acceptance additions: model finite HOST1X resources with owner and generation; buffer descriptors with format/stride/layout/cache policy and fences; timeouts quarantine resources until completion or recovery; compare CPU reference images for clear/blit/scale/format conversion. Preserve audio, display, recording and native game resource ownership. Separate decode/compose/encode capability and fallback. No new drivers imported, no tests executed, no hardware access. External reports are not EXODUS physical acceptance.

## R05 — Memory ownership, EMC and emulator evidence (2026-11-27)

| Source | Observed | Implication |
| --- | --- | --- |
| [deko3d Primer](https://github.com/devkitPro/deko3d/blob/master/Primer.md), blob 27956c9e1a4a029bc88218355ef30419bd0d8fb2, lines 243–315, 402–419 | CPU and GPU cache policies separate; CPU flush and GPU invalidation required for cached producer; GPU-to-CPU path recommends avoiding CPU-cached buffers; fence can flush GPU writes; internal ring reuse waits on fences. | Shared physical memory is not automatically coherent or zero-copy safe. Track producer, cache policy, visibility, completion, format and generation. Reject reuse before fence completion. |
| [Linux Tegra210 EMC](https://github.com/torvalds/linux/blob/master/drivers/memory/tegra/tegra210-emc-core.c), blob b645191d8a0460e4b5c3a14544aebb51d57ee1cd, lines 560–585, 724–727, 1950–1958 | Periodic memory training timer, default interval 100 ms, enable/disable depending on timing profile. | EMC is stateful and cannot be treated as independent per-OS clock; single owner of training and frequency policy. No direct timing or voltage experiments. |
| [Hekate L4T](https://github.com/CTCaer/hekate/blob/master/bootloader/l4t/l4t.c), blob f4742965feb451f0f4fcc927d93a8598bc572d41, lines 736–762 | T210B01 DRAM-ID-specific timing table selection. | Board and memory SKU must be explicit evidence dimensions; not universal settings or proof of extra physical RAM. |
| [Switchroot bootstack documentation](https://gitlab.com/switchroot/bootstack/documentation) | Separate T210 Minerva and T210B01 BPMP dependencies. | Do not extrapolate firmware-level behavior across silicon revisions. |
| [RCM-Emulator DESIGN.md, pinned commit 6d5ccb53](https://git.niklascfw.de/OmniNX/RCM-Emulator/src/commit/6d5ccb5328d881bc9c7226377db365a660c75b8b/DESIGN.md) | Author describes EMC bank model, DRAM mode-register behavior, simulated clock handshake, SD/eMMC tuning, PMIC and GPIO; several model gaps remain, including physical DLL delay and electrical behavior. | Strong candidate for bootloader regression tests, not a calibrated performance simulator or physical dual-kernel proof. Use synthetic media only; no real keys or personal storage images. |
| [Linux interconnect proposal v4, September 2025](https://lkml.rescloud.iu.edu/2509.2/04794.html) | Proposes bandwidth requests for Tegra210 EMC and describes memory bus configurations. | Candidate QoS design reference, not assumed merged. Direct linux-next fetch timed out; inspected mainline blob separately. |

Host acceptance contract for existing graph: (1) model distinct CPU/GPU cache domains and dirty/visible state; (2) producer release, completion, consumer acquire with appropriate cache maintenance; (3) test stale generation, wrong layout, invalid size, premature reuse, timeout, and fallback copies; (4) frequency/training transition is exclusive and bounded, no independent concurrent writers; (5) emulator tests with synthetic SD/eMMC images, no real user data, record source version and known model gaps. No emulator execution, driver import, timing changes or physical console access in R05. 

## R06 — RCM reference execution gate (2026-11-27)

Pinned upstream: [Creased/RCM-Emulator 6e727418](https://github.com/Creased/RCM-Emulator/tree/6e7274187861172d67e6ce2d123b89fec31bffc1). Verified [upstream CI run 43427382765](https://github.com/Creased/RCM-Emulator/actions/runs/43427382765) via job and step metadata: Linux build, smoke, regression suite and hwtest end-to-end succeeded; Windows build and smoke succeeded. This is third-party emulator evidence, not EXODUS execution. The emulator models BPMP plus CPU0 A57 and can reach Linux core initialization; it does not boot Horizon, model full SMP or establish dual-kernel/ADSP coexistence. Unknown MMIO may be simple register storage, not accurate peripheral behavior.

Local preflight: compiler/make present; Unicorn and SDL2 development packages absent; GitHub DNS resolution failed. No local emulator execution. Added dedicated read-only-token CI workflow `.github/workflows/tegra-rcm-reference.yml`, pinned public source, synthetic self-contained upstream tests, timeout, log artifact; no real NAND images, keys, device access or deployment. Own CI result pending verification. Do not equate upstream test success with Tumor acceptance.

## R07 — EXODUS reference CI verified (2026-11-27)

[Run 46014071405](https://github.com/Garyu-tt/EXODUSx1/actions/runs/46014071405), [job 140556598707](https://github.com/Garyu-tt/EXODUSx1/actions/runs/46014071405/job/140556598707), tested commit 13c33cd073d986eac765ee6ae6452817552931b4, upstream 6e7274187861172d67e6ce2d123b89fec31bffc1, Unicorn package 2.1.4-3. Job metadata and 1072-line raw log inspected. Five explicit PASS markers:

| Suite | Result | Evidence relevance to existing #349 DAG |
| --- | --- | --- |
| ccplex | PASS | Reference CPU0 startup/parking; boot-context remains physical gate |
| kernelcpu | PASS | Synthetic A57 execution substrate only; not FreeBSD remote syscalls or SMP |
| se | PASS | Emulator cryptographic reference; not physical device security |
| usb | PASS | Synthetic USB behavior; not hardware throughput or ownership |
| display | PASS | Reference display/VIC; gpu-mediation remains physical gate |

[Log artifact 15205301401](https://github.com/Garyu-tt/EXODUSx1/actions/runs/46014071405/artifacts/15205301401), ZIP size 2769 bytes; reported SHA256 39a327c1289b9d8f4d1fc16dc123f35e9b9c4199c6e3dd5053fa9104fda56857, not independently downloaded/hashed.

Compared existing graph on integration/tumor-amp-execution-graph: ARMv7 FreeBSD QEMU boot and mixed-width QEMU IPC already have evidence; retain those scopes. RCM does not emulate A9 ADSP execution, all four A57 cores, or Horizon. No physical gate may be promoted. RCM reference evidence should be linked to boot-context, a57-execution-substrate and gpu-mediation without changing their admission status. Next useful test is an EXODUS-specific synthetic payload exercising CPU0-owned memory and transport state, distinct from upstream regression. Do not duplicate the existing DAG or claim a hardware-compatible payload. No device access.

## R08 — EXODUS-specific mixed-width fixture

Added exodus/tests/rcm_ipc_reference.py (59e0b56) and extended the reference CI (1eff8cd). The adapter verifies the upstream revision and unique source anchors, then modifies only the disposable upstream kernelcpu test. It exercises an ARM32 BPMP host submitting three fixed-width words and an AArch64 CPU0 replying with a 64-bit carry split into two words, echoed sequence and generation mismatch flag. Original upstream kernelcpu assertions remain required. CI additionally requires a positive EXODUS marker, rejects a negative marker and preserves text logs, not deployable binaries. This tests a single synthetic request/reply; the mismatch flag is not a complete stale-request rejection protocol. The emulator does not model ADSP Cortex-A9, physical cache coherence, concurrent Horizon, or production transport. No physical access. Own R08 test result pending; R07 remains the last verified run until new CI completes.

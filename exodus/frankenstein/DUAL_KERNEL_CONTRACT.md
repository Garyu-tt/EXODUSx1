# Tumor R17: dual-kernel execution contract

Canonical owner decision: #103. Implementation queue: #105. This document
defines the target, not a claim of physical coexistence. Existing QEMU evidence
remains reference-only. No boot payload, firmware changes or raw register writes
are authorized by this document.

## Physical target clarification (2026-11-26)

Owner direction: [#103](https://github.com/Garyu-tt/EXODUSx1/issues/103#issuecomment-7176567653).
The physical candidate is asymmetric multiprocessing (AMP): retain all four
Cortex-A57 application cores for Horizon and investigate the separate
Cortex-A9 ADSP for resident FreeBSD/armv7. A53 exploration stays deferred.
The ARM64 Bao/QEMU results below are reference controls, not the physical
boot design. EL2 mediation on the application cores is not an assumed
requirement for AMP; an authorized ADSP bootstrap and device arbitration
still need proof.

Admission sequence for this candidate:
- Verify exact board revision and ADSP availability, supported startup,
  instruction set, MMU, usable RAM, timer/IRQ and safe recovery.
- Build a separate FreeBSD armv7 kernel and minimal userspace. ARM64
  artifacts are not installable on this core. FreeBSD armv7 is Tier 2.
- Demonstrate a bounded, versioned mixed-width IPC channel. In particular,
  verify aligned atomic width, ordering and cache visibility across domains;
  ARM64 acquire/release evidence cannot establish ADSP coherence.
- Keep audio and Horizon service behavior intact. Device ownership,
  graphics/compute execution placement and Steam ABI remain independent
  gates; do not assume an ARM64 client can execute on the Cortex-A9.
- Admit a clean SD install only when a target-specific bootable candidate,
  recovery procedure and exact SD destination are verified. Preserve eMMC.

Sources: [FreeBSD platform support](https://www.freebsd.org/platforms/),
[FreeBSD ARM project](https://www.freebsd.org/platforms/arm/),
[Linux Tegra210 SoC description](https://kernel.googlesource.com/pub/scm/linux/kernel/git/arm64/linux/+/06898e5320b5c12dd8c21d0ee8d5a68347001746/drivers/soc/tegra/Kconfig).
No physical hardware test or firmware change is asserted here. The following
historical reference topology and results are retained for reproducibility.

## Bootstrap gate

User-visible order: Horizon starts, then FreeBSD starts and becomes the control
plane. The required privileged mediation may have to be established by an
authorized boot environment **before** Horizon enters its kernel. Launching a
second privileged kernel from an ordinary NRO is not a supported assumption.
The candidate topology is two isolated kernel domains under a privileged
mediator, with mediated devices and bounded shared channels. The actual
privilege level, board-specific boot support and supported recovery path remain
unknown. Do not assume EL2 is available, and do not bypass security boundaries.
If the authorized platform cannot support coexistence, report BLOCKED rather
than calling dual boot or a BSD compatibility shim dual-kernel.

## Ownership contract

| Resource | Initial owner / required behavior |
|---|---|
| CPU | Mediator schedules both domains; no uncontrolled core ownership. Measure latency. |
| Physical RAM | Disjoint reservations and explicitly mapped shared buffers. Budget against actual target-available RAM. |
| Interrupts, timers and DMA | Mediated or assigned exclusively; prove isolation before enabling real devices. |
| GPU, display, HID | Initially keep Horizon's native game path. BSD rendering uses a validated mediated path or explicit exclusive handoff. No concurrent direct device ownership. |
| Wi-Fi | BSD owns logical network services and SSH endpoint; actual hardware driver owner is TBD and may require mediation. |
| SD / persistent storage | One writer per filesystem; separate volumes or validated file service. Never mount a writable filesystem in both kernels concurrently. |
| Nintendo title lifecycle | Horizon retains native execution, applet services and permissions; FreeBSD requests authorized actions through the bridge. |
| UI / configuration | FreeBSD service plane owns desired settings and session state; target UI is Steam Gaming Mode and KDE. |

## Runtime and admission gates

1. **Physical context**: identify exact console and authorized boot context;
   record evidence source and distinguish USB mass storage, EL0 diagnostic and
   privileged platform evidence. No raw writes. EL0 cannot independently
   establish EL2 ownership.
2. **Mediator**: demonstrate supported privileged startup, protected memory,
   disjoint CPU/RAM domains, IRQ/timer routing and DMA isolation using controlled
   tests in reference environments first. Hardware result required separately.
3. **Concurrent boot**: Horizon stays alive while a real ARM64 FreeBSD kernel
   starts and reports independent uptime. Preserve both identities and
   timestamps; repeated heartbeats must overlap without reboot or simulated
   kernel output. Establish bounded IPC and verify malformed message rejection,
   timeout, disconnect and peer restart. No raw memory or generic privileged
   shell capability.
4. **Resident profile**: start FreeBSD rc.d, OpenSSH and minimal management
   services. Keep them running during an authorized native title session.
   Observe SSH command/heartbeat before, during and after launch; record RAM,
   CPU, frame-time impact and service PIDs. Nonessential graphical processes
   may be suspended or stopped, but the BSD kernel and admin services stay up.
   Deep sleep and power-off are outside the always-on promise.
5. **UI/provider integration**: same catalog and settings registry for Steam
   Gaming Mode and KDE; prove display/HID handoff, title return, saves and
   recovery. Steam client and game ABI compatibility are separate admission
   gates. A closed binary cannot be presumed portable.
6. **Release**: physical evidence for every required node, measured resource
   budgets and explicit recoverable installation scope. No firmware changes
   or release promotion from source audit or QEMU results alone.

## Work allocation

Prioritize physical context, privileged mediation and CPU/RAM/IRQ/DMA
isolation. Continue independent FreeBSD reference, graphics and upstream
manager builds without claiming Switch acceptance. Retain #89 typed Horizon
provider work but stop duplicate Horizon SSH/TLS production services: external
SSH/SFTP lives on BSD. Reuse existing diagnostics only as evidence and where
the selected inter-kernel transport needs them.

The machine-readable graph.json is the dependency DAG; this contract explains
its hardware gates. #55 applies to all interfaces. Feature freeze: these
nodes elaborate existing #103/#105 requirements, not new product features.

## R19 candidate assessment and heartbeat contract

- [Jailhouse upstream](https://github.com/siemens/jailhouse) and [Jetson TX1/TX2 port](https://github.com/evidence/linux-jailhouse-jetson) establish a related Tegra partitioning precedent, not Switch support. Jailhouse requires a Linux control OS; use as an architectural donor only, not a third production OS.
- [FreeBSD 15.1 vmm(4)](https://man.freebsd.org/cgi/man.cgi?format=html&query=vmm&sektion=4) documents EL2 at boot and GICv3 on arm64. [Upstream Tegra210 device tree](https://code.googlesource.com/linux/torvalds/linux/%2B/6f771ce6c3bb2c4dc9d348b578871c7adb8b285b/arch/arm64/boot/dts/nvidia/tegra210.dtsi) describes GIC-400 (GICv2). Do not assume stock bhyve can host this physical platform. Actual Switch revision and virtual interrupt support remain to be established.
- [seL4 TX1 documentation](https://docs.sel4.systems/Hardware/jetsontx1.html) explicitly excludes SMP, SMMU and Hyp-mode in that kernel port; not a turnkey mediator for this target.
- `ipc_contract.py`: 24-byte fixed-size, big-endian reference heartbeat with version, role, generation and strict sequence; reject malformed/replayed/stale messages and close on local 3s timeout or clock rollback. Locally established generation and authenticated endpoint identity are assumptions of the reference model, not authentication provided by the frame itself. Two fresh heartbeats are necessary but not sufficient to prove independent kernel execution. No actual shared-memory driver, kernel, hardware mapping or privileged transport is implemented.
- Local Python tests: 7/7 groups PASS, including nine malformed-frame variants; exact Git blob 4a89d74f6ad97d0764e213b67498ab02f77e3791. CI matrix pending. Timeout is a test parameter, not a measured hardware budget.

## R23: candidate-specific CPU topology admission

Source audit of Bao v2.0.0: [TX2 platform.mk](https://github.com/bao-project/bao-hypervisor/blob/v2.0.0/src/platform/tx2/platform.mk) explicitly selects Cortex-A57 and GICv2 (blob 32744606997f4a912c4bb2938f7d196a375c3841). [QEMU virt platform.mk](https://github.com/bao-project/bao-hypervisor/blob/v2.0.0/src/platform/qemu-aarch64-virt/platform.mk) defaults to Cortex-A53 and GICv3 (blob 126ee1aabcf0b3361b9e98be0c84af106bfc9db8). TX2 is a donor for architecture, not a Switch board definition; its device addresses, memory map and CPU topology cannot be reused blindly. Bao's reference static partitioning uses assigned physical CPUs; the EL2 mediator need not consume a dedicated physical core. For reference CPU assignments, require an explicitly supplied available-core topology and reject unknown, duplicate and non-integer identifiers. A 3+1 assignment without a dedicated mediator core is only a synthetic test; hardware budgets and scheduling policy remain unproven.

Local partition contract: 16/16 tests passed, exact blob 70549400b87280f5f73fe5ddbe6546b69fe9ac3a. No Bao build, QEMU runtime, hardware changes or dual-kernel boot performed. R22 CI and physical privilege gates remain pending. Next admission: run reference EL2/EL1 checks, then an upstream-supported two-guest Bao configuration on an isolated reference platform; independently evaluate GICv2 and real FreeBSD guest compatibility before target hardware.

## R25: actual reference hypervisor execution (2026-11-25)

Independent Linux test host, isolated extracted Debian packages; no system package installation, production service changes or Switch writes. QEMU 10.0.13, GCC 14.2.0. Exact Git blobs for EL probe and Bao guest/config inputs verified before compilation. Cortex-A57/GICv2 QEMU virt with virtualization on/off gave the respective single markers `TUMOR_REF: EL2` and `TUMOR_REF: EL1_BLOCKED`; both stopped on the intended timeout. ELF SHA256 d1baea6ce39e426efae61e076a9e22d48c6d884da935f778edfdf162dc7b9290.

Bao pinned commit 0af4a1ab558ad60c9658af4f43756c4536dd3141 built successfully after supplying cross libc development headers. Direct raw boot and generic loader without firmware both timed out without a success marker; neither is accepted. Pinned Bao TF-A fork e4ffdd76c3c4b1488eb9c290e5b5c676f68f6b38 built BL1/BL2/BL31/FIP with minimal reference BL33 that transfers control to Bao at the QEMU-only address. QEMU virt secure=on, virtualization=on, GICv3, Cortex-A53, 4 CPUs, 1024 MiB, no network. Console printed BL1/BL2/BL31, Bao Hypervisor, then `TUMOR_BAO_REF: TWO_EL1_GUESTS_IPC`; bounded timeout 124. The synthetic guests check CurrentEL and exchange a three-step shared-memory handshake. This verifies the specific reference handshake under a real hypervisor, not actual Horizon/FreeBSD kernels, DMA isolation, physical Tegra, GICv2 Bao or long-running liveness.

Reference hashes: bao.elf c32905979e1ecf9281f1d4bb8cdbeeb3d37b356c3197cf7d22774e03d937cfcd; bao.bin f4a47c38bded335e573ff5642ed968eaa57c34a4c166e3e78ee4830121c73ccb; ref-flash.bin 6d6be2d49ab2d377583305dffab58ee9fac5b54dcab8de4420da71086fc01649. Firmware binaries are reference-only, not installable on Switch. CI expanded to reproduce boot; CI result pending. Next gate: real FreeBSD guest and then independent GICv2 reference verification.

## R26: real FreeBSD kernel boot under Bao

Executed on the isolated R25 Linux test stand (QEMU 10.0.13, Cortex-A53/GICv3, four host vCPUs, 1 GiB, no networking). Downloaded the official FreeBSD 15.1-RELEASE arm64/aarch64 kernel.txz and verified its release MANIFEST SHA256 05f770c6af035fab312cd65592f1dbd1bff020c7daa75d8d4ecc96cae8f042c0. Extracted only boot/kernel/kernel; SHA256 e66c772fb7b1144c61735c18dceb1f598f87f18b5acd9781c1d78327a6f68379. Upstream [locore.S](https://github.com/freebsd/freebsd-src/blob/releng/15.1/sys/arm64/arm64/locore.S) and [std.arm64](https://github.com/freebsd/freebsd-src/blob/releng/15.1/sys/arm64/conf/std.arm64) support the Linux boot ABI FDT entry. No FreeBSD source patch applied.

The tested boot wrapper (blob 8a05fa6f251a78ef098286160e25996772d56aa7), FDT (b87cd81d7c59f5115e2e43e7adef0aa1e65d1867), Bao config (57fa0dca26b27e03146cde598d1e5f631f02c85b) and packer (7b000c917ba8d32691f596155641ed3726f31a6b; byte-identical repack verified) are preserved. The wrapper checks EL1 and completes the R25 IPC handshake with synthetic guest A before passing the FDT pointer into the kernel. Guest B gets one vCPU and 512 MiB; guest A gets three vCPUs and 64 MiB, synthetic only. The kernel reaches mountroot, including UART, GICv3, ARM generic timer, network stack initialization, entropy unblocking. Guest A halts after its handshake; no continuing peer liveness proven. No root disk, userspace, sshd or physical target execution in this test. Kernel reports 512 MiB total, 479 MiB available (not a steady-state application budget). Boot warning about missing freebsd,dts-version and initial entropy warning are recorded, not hidden.

Actual log markers: TUMOR_BAO_REF: FREEBSD_HANDOFF_AFTER_PEER_IPC; FreeBSD 15.1-RELEASE releng/15.1-n283562-96841ea08dcf GENERIC arm64; mountroot>. QEMU exit 124 expected timeout. Boot bundle SHA256 6ed5f8671cb075ff65eb6f259c26d8a98a5fa8ab0d04a97079a3355b41283fde, Bao binary be175c6c7934071cbfaf0a476f84f96844b85f8a352025befbf64f535c7d45f6, DTB 42b283f7a5350438fa5ec8920d5d1429294dfc673c10232a23c0e89d17c71c9f. Existing R25 TF-A flash reused. QEMU-only addresses; no Switch deployment. Next gate: attach isolated root filesystem, boot userspace, then test native BSD OpenSSH and ongoing peer heartbeat. GICv2 and real Horizon remain separate gates.


## R32: two stock FreeBSD kernels, hosted reference acceptance

Exact head `6dfb6d71901e6bdb54c7b9cd7d853b508c6beab1`, accepted into #153 at `ea868d32d8e5584596b867fc2cb7b13a32bb0556`. [Actual Bao job](https://github.com/Garyu-tt/EXODUSx1/actions/runs/45690774623/job/139475322473), [host checks](https://github.com/Garyu-tt/EXODUSx1/actions/runs/45690774813): 10/10 passed. QEMU virt, Cortex-A53/GICv3, 1 GiB, no NIC; two original release kernels, separate UFS1 RAM roots, configured 256 and 512 MiB regions. Headless guest PID 1 uses getpid and advances an EL0 register counter; independent guest runs authenticated OpenSSH loopback commands. Exact result `EXODUS_BAO_DUAL_FREEBSD_PASS first=13008882 second=16690836`; remote exit 23 verified. R31 synthetic peer control also passed. This is concurrent OS execution, not independent exhaustive stage-2/DMA isolation testing or Horizon admission.

Runtime IPC next gate: a narrowly scoped shared-channel device in each guest using the reserved mediator-owned region, with explicit kernel-approved resource mapping and bounded typed heartbeat exchange after both kernels boot. No /dev/mem or arbitrary physical-address access. Validate both directions, timeout, malformed/replayed messages and restart before admission. Do not equate the pre-kernel boot handshake or QMP counters with runtime IPC. Physical gates remain open.

Separately [graphics job](https://github.com/Garyu-tt/EXODUSx1/actions/runs/45678629911/job/139432868045) confirms Wayland and Gamescope source package build and host checksum verification ([artifact](https://github.com/Garyu-tt/EXODUSx1/actions/runs/45678629911/artifacts/15058370774)); compositor rendering remains unverified. These evidence records do not merge the independent graphics implementation branch.

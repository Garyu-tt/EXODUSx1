# Tumor R11 — Tegra and dual-kernel admission

## Existing native code (source evidence, not target acceptance)

FreeBSD 15.1 has a Tegra210 platform tree:
- [ARM64 Tegra210](https://github.com/freebsd/freebsd-src/tree/releng/15.1/sys/arm64/nvidia/tegra210): CAR clocks, PLLs, pinmux, PMC, thermal/CPU frequency, XUSB pad controller, MAX77620 regulator/GPIO/RTC.
- [std.nvidia](https://github.com/freebsd/freebsd-src/blob/releng/15.1/sys/arm64/conf/std.nvidia): SOC_NVIDIA_TEGRA210, FDT, SDHCI, XUSB firmware and UART.
- [NVIDIA DTB makefile](https://github.com/freebsd/freebsd-src/blob/releng/15.1/sys/modules/dtb/nvidia/Makefile): arm64 builds tegra210-p2371-2180 and tegra210-p3450-0000. No Nintendo Switch DTB in this build list. Switch Lite board-specific data and SoC differences require separate validation.
- [ARM64 vmm](https://github.com/freebsd/freebsd-src/tree/releng/15.1/sys/arm64/vmm): VHE and non-VHE implementations exist. This is FreeBSD as a hypervisor host, not proof that Horizon can be hosted, nor that EL2 is available on the console.

Pinned donors:
- [NintendoSwitchPkg](https://github.com/imbushuo/NintendoSwitchPkg/tree/09c3ade486aa76b53557d930ed291760cea41cb0): experimental EDK2 with reported CPU/GIC/timer, SD, clocks, GPIO, power and framebuffer support. GPL-2.0 repository. Direct boot is an alternative research path, not simultaneous Horizon execution; no confirmed Switch Lite/FreeBSD boot.
- [Atmosphere](https://github.com/Atmosphere-NX/Atmosphere/tree/3e8915011e1de89f595462d929c1cc3294470896): README describes Thermosphere EL2 EmuNAND concept. No proof of a general-purpose dual-guest hypervisor or deployed EL2 support. GPL-2.0 repository.
- [Mesa Horizon port](https://github.com/frankyfife/mesa-switch_ff/tree/0ff52079ca9e9e837ed27eb95a8d239f439a1864): upstream-derived Mesa 26.2.4 with libnx-backed Nouveau/NVK/NVC0 and static NRO/NSO build. README explicitly excludes external-memory FD/DMA-BUF support and describes loaderless Vulkan. Not a drop-in FreeBSD Vulkan ICD or Linux DRM render node. Component licensing requires review.

## AMP physical admission (supersedes R11 physical EL2 topology)

The physical candidate is asymmetric multiprocessing: Horizon retains all four Cortex-A57 application cores; a separate Cortex-A9 ADSP is the candidate for FreeBSD/armv7. A53 remains deferred. Bao/EL2 is an ARM64 reference test, not a physical prerequisite or a substitute for board support. Existing ARM64 Tegra drivers are donor source, not ARMv7 ADSP drivers.

Reference evidence:
- FreeBSD 15.1 ARMv7 boot and native codec/mailbox tests: run 45717815165, job 139568302191, QEMU virt Cortex-A15, native FreeBSD ELF32. This does not certify Cortex-A9 platform support.
- Mixed-width codec/mailbox: same run, job 139568302123, QEMU Linux userspace Cortex-A9/A57, both role assignments and full-width regression. This does not certify cross-OS shared memory or physical coherency.

Physical admission remains blocked until all of these are supported by board-specific evidence:
- Exact Mariko/Switch Lite revision and ADSP CPU features, including MMU, exceptions, timer, IRQ, addressable memory and supported startup/reset ownership.
- Separate memory regions and access permissions; demonstrate cache visibility, interconnect exclusive-monitor behavior and notification ordering. A compiler's lock-free atomics or a QEMU result alone cannot certify this.
- Preserve Horizon audio initialization, rendering/output, Opus, suspend/resume and stock recovery. ADSP is a used resource, not a spare application core. No watchdog bypass or firmware replacement is implied.
- Establish where graphics and compute execute; existing ARM64 packages cannot run natively on ARMv7. Retain the single-owner device rule.
- Only a verified target-specific candidate and recovery route can admit a clean SD install. Internal eMMC is excluded.

## Historical ARM64 reference admission sequence (R11)


1. Reference FreeBSD QEMU boot: PASS, run 45495215979, not Tegra hardware.
2. Physical board support: inventory existing drivers against the exact Switch Lite revision, read-only model and memory evidence; no register writes.
3. Historical ARM64 reference coexistence: verify EL2 ownership for the Bao-style topology. For the physical AMP candidate use the separate admission criteria above. Do not assume a bootloader can simultaneously run both OSes.
4. Choose hardware ownership before driver integration. Candidate mediated GPU path: Horizon retains GPU ownership, BSD receives a versioned bounded render interface. Alternate direct BSD GPU driver path requires explicit exclusive ownership and proof of recovery. No simultaneous direct MMIO by both kernels.
5. Prove independent kernel uptime and IPC with bounded shared memory, then authorized native-title launch through existing Horizon provider work (#30–36, #61). SSH reachability must survive UI/session switches. Require physical test and recovery plan before any boot changes.

No boot image, firmware patch, register access, security-state change or hardware test is included in R11. All paths above are admission gates, not completed capabilities. Keep source-port work independent of hardware risks.

## Physical USB admission (R16; read-only)

A connected USB cable is not hardware acceptance. Before any target-side test,
record the authorized laptop identity, observed USB mode and device class.

- On the Windows laptop, compare present USB/PnP devices, disk inventory and
  mounted volumes. Read-only queries: `Get-PnpDevice -PresentOnly`,
  `Get-Disk`, `Get-Partition`, `Get-Volume`. Correlate disk numbers to
  device instance IDs and capacity; do not select a disk based on its drive
  letter or capacity alone.
- Distinguish USB mass storage, RCM/recovery, MTP, and an actual target shell.
  A mounted volume does not establish CPU execution, EL2 ownership, board
  revision or access to Horizon services. If the exposed UMS backing store
  (SD versus eMMC or partition) is ambiguous, stop at enumeration.
- Record confirmed console model/board revision only from authorized
  device-specific diagnostic evidence. Do not infer it from the laptop's
  USB class or the QEMU CPU model.
- The R16 resident archive is a FreeBSD ARM64 reference userspace bundle,
  not an NRO, boot payload, firmware image or Switch installer. Do not
  copy it over a bootloader or attempt to launch it directly under Horizon.
- No format, mount-mode changes, raw writes, payload injection, boot
  configuration edits, NAND access beyond device enumeration or security
  changes are part of this admission. A recovery plan and exact target
  identification are required before a separate physical boot experiment.

Record USB inventory, target execution, boot persistence and graphics as
independent evidence. A missing transport is BLOCKED, not a target failure.

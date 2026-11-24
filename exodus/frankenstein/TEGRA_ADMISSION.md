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

## Admission sequence

1. Reference FreeBSD QEMU boot: PASS, run 45495215979, not Tegra hardware.
2. Physical board support: inventory existing drivers against the exact Switch Lite revision, read-only model and memory evidence; no register writes.
3. Coexistence feasibility: verify EL2 ownership and supported boot architecture using source and authorized diagnostic evidence. Do not assume a bootloader can simultaneously run both OSes.
4. Choose hardware ownership before driver integration. Candidate mediated GPU path: Horizon retains GPU ownership, BSD receives a versioned bounded render interface. Alternate direct BSD GPU driver path requires explicit exclusive ownership and proof of recovery. No simultaneous direct MMIO by both kernels.
5. Prove independent kernel uptime and IPC with bounded shared memory, then authorized native-title launch through existing Horizon provider work (#30–36, #61). SSH reachability must survive UI/session switches. Require physical test and recovery plan before any boot changes.

No boot image, firmware patch, register access, security-state change or hardware test is included in R11. All paths above are admission gates, not completed capabilities. Keep source-port work independent of hardware risks.

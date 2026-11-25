# EX-013: Proton NX (experimental)

Goal: evaluate and port useful Valve Proton gaming compatibility changes to the native Horizon runtime, with an optional full Proton-compatible runtime if practical. No Linux reboot or companion PC for the final integration. Do not replace the known working Wine runtime until measured and reversible.

## Verified candidate sources
- Valve Proton ARM64 build documentation: https://github.com/ValveSoftware/Proton
- Cortex-A57 ARMv8.0 Linux prototype: https://github.com/Azkali/proton-arm64-a57/releases/tag/11.0-1-beta5-v80-standalone1
- Switchdeck Linux release based on Proton 11.0-1 (d2bedfa): https://github.com/SildurFX/Switchdeck-Extras/releases/tag/Proton-11.0-1-Armv8.0 ; reports Vulkan compatibility patches and unresolved 32-bit graphical bugs.
- Native Horizon Wine/FEX/Box64/Mesa/DXVK/VKD3D-Proton: https://github.com/autorunhq/autorun ; documents a single Windows process limitation.
- Steam sign-in and on-device downloading candidate for EX-006: https://github.com/plv890/CorkDeck ; experimental separate shell, not actual HOME. Audit license enforcement and Steamworks implementation, reuse only legitimate authorized interfaces; no DRM bypass.
- Actual 4IFIR installed runtime identity still unverified. Candidate sources are not installed component evidence.

## Port strategy
1. Identify actual installed runtime and toolchain, reproduce a clean native baseline. Pin source SHAs and submodules and review licenses. Obtain reproducible baseline build and hardware logs.
2. Compare Valve wine submodule changes with Autorun Horizon-specific patches. Classify portable PE/API/game fixes, Horizon host changes, graphics dependencies, and Linux-only launch/runtime requirements. Prefer small independently testable patch series, not blind overwrite of Wine DLLs.
3. Build for Cortex-A57 ARMv8.0-A; audit for ARMv8.1 LSE instructions and later ISA features, use supported atomics and test real hardware. Review FEX ARM64EC/x86 and 32-bit WoW64 separately.
4. Reuse existing native Mesa/DXVK/VKD3D graphics paths. Test candidate DXVK versions against actual Maxwell NVK features; do not assume Linux Tegra vendor Vulkan patches apply to Horizon Mesa. Implement only supported workarounds.
5. Investigate native alternatives for Linux syscalls, ELF loading, wineserver and process/IPC assumptions, futex-based synchronization and Steam Linux Runtime containers. Keep EX-003 multi-process work explicit; do not fake successful APIs. An incremental Proton patchset is a valid milestone, not a full Proton port.
6. Integrate selectable Wine/Proton-NX per-game backend into EX-006 actual HOME and EX-007 Control Center. Keep independent prefixes and caches; preserve saves, original runtime and rollback. Use only user-owned games and legitimate authentication/distribution.
7. Compare matched real-device runs with EX-008: startup, correctness, mean FPS, 1% low, P99, input latency when measurable, memory, power and stability. Include GTA San Andreas logo hang and Steam installer/client regressions, one simple x86 title and one supported x64 title. No performance promises without measurements.

## Acceptance
- Source/build manifest and license provenance, ARMv8.0 audit, actual Switch boot of experimental runtime.
- At least one legitimately owned title reaches interactive gameplay locally; separate 32-bit and 64-bit evidence and explicit unsupported results.
- Compare against existing runtime at identical settings, no hidden quality reductions or generated frames counted as native.
- No regression to native Switch titles; optional per-title backend, reversible installation, no default change until evidence.
- Full Proton-NX claim requires actual runtime and necessary process/service support, not merely inclusion of DXVK/VKD3D-Proton.
- Steam integration remains EX-006: direct account login, owned library, authorized downloads, actual HOME, no PC.

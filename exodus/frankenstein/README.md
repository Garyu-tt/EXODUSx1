# Frankenstein R01: real SteamOS component port to FreeBSD ARM64

Owner direction: #105 and #103. This is an upstream-component port, not a replacement UI. The reference VM is intentionally separate from Horizon and Switch firmware.

## Reproducible entry point

On a disposable Linux build host with QEMU, edk2, dosfstools, mtools, curl and xz:

```sh
sh exodus/frankenstein/run-reference.sh /absolute/path/to/evidence
```

The host downloads the official FreeBSD 15.1 ARM64 BASIC-CI image and verifies the publisher's HTTPS checksum list. It expands a temporary disk, injects a NoCloud seed containing the guest build script, and boots QEMU virt with Cortex-A57 CPU emulation. No inbound SSH forwarding; the guest disables its test-image SSH daemon. The guest reports actual kernel/architecture, fetches the pinned Ports tree, captures the real make dependency variables and attempts a Wayland source package build. The build uses binary packages only for dependencies; their versions are recorded by pkg -vv and build logs, not fully locked yet. The CI artifact includes the serial log and checksum manifest, not redistributed OS binaries.

## Port classification

graph.json is the initial integration DAG, not an exhaustive package-dependency closure. All eleven candidate port Makefiles are verified at the pinned FreeBSD Ports commit; a recipe is not evidence of a successful ARM64 build. Gamescope is 3.13.8 at that pin. Linux Steam Utils and Wine Proton are amd64-only. Proprietary Steam ARM64 client and its ABI/ISA dependencies remain unverified. The reference CPU model is not a Tegra device or GPU emulation. QEMU boot, nuageinit, package downloads and build results require CI evidence.

Sources: https://github.com/freebsd/freebsd-ports/tree/98796bd539ea8f03adb7c9d6f9f3a3d54dcca553 ; https://download.freebsd.org/releases/CI-IMAGES/15.1-RELEASE/aarch64/Latest/ ; https://github.com/freebsd/freebsd-src/blob/releng/15.1/release/tools/basic-ci.conf ; https://github.com/ValveSoftware/gamescope ; https://github.com/shkhln/linuxulator-steam-utils .

Next gate: actual ARM64 Wayland build, followed by Gamescope dependency closure and headless/nested compositor validation. Keep BSD kernel/Horizon coexistence, Tegra GPU support, original Steam client and inbound SSH service on the console as distinct gates. No changes to Switch firmware or SD card in R01.

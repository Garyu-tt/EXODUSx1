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

## R02: Gamescope source package

R01 reference CI was still running when R02 was prepared. R02 runs independently, retains the Wayland gate, then expands actual FreeBSD Ports dependencies, verifies distfiles and patches with the port checksum target, builds the original Valve Gamescope 3.13.8 port revision 11, installs the resulting local package in the disposable guest and checks ELF identity and --help. Binary dependencies are permitted, but the Gamescope target itself is compiled from source. The pinned recipe uses wlroots 0.17, libliftoff, libdisplay-info, libxkbcommon, Vulkan headers/loader, SDL2, X11, and optional PipeWire (default on). Its FreeBSD patch adjusts wlroots discovery and libdisplay-info version selection. This is a build/CLI test, not rendered compositor acceptance. The guest has a larger disposable disk and longer bounded build timeout. No GPU, actual Switch boot, Steam client, or Horizon integration claimed. CI is pending.

## R03: upstream SteamOS Manager protocol and daemon audit

Pin the MIT-licensed source mirror of Valve SteamOS Manager v26.4.1 at a0d399defebd60db30ba38325d9cf47dbda49bee (https://github.com/evlav/steamos-manager/tree/a0d399defebd60db30ba38325d9cf47dbda49bee). Upstream project: https://gitlab.steamos.cloud/holo/steamos-manager/. The original proxy crate exposes Valve D-Bus API interfaces, including session management. The actual session implementation uses systemd unit properties/signals and SDDM configuration; full daemon Cargo dependencies include Linux-specific inotify, udev, input-linux and linux-cec. Audit replacement boundaries rather than pretending those APIs work on BSD. Candidate providers: rc.d/service supervisor, kqueue, devd, evdev, explicit CEC capability. D-Bus proxy compilation does not implement the daemon.

Run `sh exodus/frankenstein/run-reference.sh /absolute/evidence manager` to boot an isolated FreeBSD guest and attempt a locked native Cargo build of the original proxy and macros crates. After a successful proxy build, collect the unmodified daemon's cargo check return code and diagnostics; a nonzero daemon check is an explicit unresolved port blocker, not a daemon pass. The R03 CI workload selects manager only; R02 retains the graphics workload. R01 and R02 CI were in progress when R03 was prepared. No source compile or runtime success yet observed. No Switch changes.

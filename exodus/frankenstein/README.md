# EXODUS Tumor R01: real SteamOS component port to FreeBSD ARM64

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

## R04: inbound SSH and session-intent reference gate

Run `sh exodus/frankenstein/run-reference.sh /absolute/evidence ssh`. This independent workload boots the same disposable FreeBSD ARM64 guest and uses its actual base OpenSSH server, not a mock or outbound-only client. An isolated loopback listener, dedicated unprivileged test user, ephemeral host/client keys, strict host-key pinning, public-key-only authentication and forwarding disabled are configured. Tests accept the paired key, reject an unknown key, change the persisted desired session between desktop and game via SSH without reboot or replacing the server PID, reject invalid modes, then revoke the key and reject a fresh login. The adapter only records desired session intent; actual compositor/SDDM switching, upstream SessionManagement1 D-Bus service integration, hot-reload rollback and existing-session revocation are not implemented. The reference listener does not expose a host or LAN port. No production credentials or console files are touched. CI is pending; no successful authentication claimed yet.

## R05: isolated kernel boot gate and serial diagnostics

The four earlier CI workflows reached their reference workload step but no guest serial evidence was available yet. Isolate boot and first-boot script execution from downloads/builds of packages: run `sh exodus/frankenstein/run-reference.sh /absolute/evidence boot`. The guest checks FreeBSD/aarch64 identity, version, CPU and memory, disk layout and root filesystem, then powers off. No package installation. Boot gate timeout is 12 minutes after QEMU starts; serial logs stream to CI and the final tail is printed on failure. The official FreeBSD 15.1 nuageinit rc script mounts the seed without a read-only option (https://github.com/freebsd/freebsd-src/blob/releng/15.1/libexec/rc/rc.d/nuageinit), so use a writable disposable seed rather than the previous read-only virtual disk. This is a suspected boot blocker, not a confirmed root cause. All other workload modes inherit the corrected launcher; prior running workflows are unchanged. No successful guest boot asserted. No device changes.

## R06: resident POSIX session configuration adapter

A small C process accepts owner-only regular configuration files (no final symlink, no group/other write permissions, bounded input), containing generation and desired mode. SIGHUP validates the candidate and activates strictly newer generations without replacing the process; malformed, stale or unsafe configurations leave the running generation unchanged. SIGUSR1 prints status; SIGTERM shuts down. Local Linux AMD64 compilation with -Wall -Wextra -Werror and integration test passed before commit. The same tests are added to AMD64/ARM64 Linux CI. This is an initial local adapter for the future SteamOS Manager BSD backend, not a replacement for the upstream daemon. It does not launch a compositor, expose D-Bus or network sockets, provide durable rollback across daemon restarts, or authenticate an SSH client. Run only unprivileged in an owner-controlled directory. Native FreeBSD acceptance is pending; console unchanged.

## R07: SteamOS Manager D-Bus configuration adapter

The optional GDBus build shares R06's validated configuration parser and uses the exact upstream SessionManagement1 and Manager2 interface names and signatures. On a private reference session bus, DefaultLoginMode reads the active desired mode and its setter atomically publishes a newer configuration generation; ReloadConfig validates newer on-disk generations. PropertiesChanged reports accepted mode updates. Unknown/malformed settings and stale generations are rejected. The program refuses root execution. Use an owner-controlled directory with one writer; rename provides atomic visibility, not durable crash rollback or concurrent writer serialization. This is a reference compatibility adapter, not Valve's full daemon. Real graphical switching, actual hardware identity and desktop discovery are intentionally unsupported or empty until verified providers are connected. No system bus service is installed. The integration test exercises D-Bus calls, persistence, rejection and process continuity on AMD64/ARM64 Linux. FreeBSD and physical Switch verification remain pending. No successful R07 test is asserted before CI evidence.

## R08: native FreeBSD integration and corrected upstream workloads

R05 boot evidence: https://github.com/Garyu-tt/EXODUSx1/actions/runs/45495215979 ; kernel 15.1-RELEASE releng/15.1-n283562-96841ea08dcf GENERIC arm64, ARM Cortex-A57 r1p0, 4 CPUs, 4263301120 bytes physical memory, EXODUS_FREEBSD_BOOT_PASS and EXODUS_GUEST_RESULT=0. R06 Linux AMD64/ARM64 service tests and R07 Linux AMD64/ARM64 D-Bus tests also passed: runs 45495521234 and 45496077072. This is QEMU virt, not Tegra or Horizon coexistence.

R08 copies the exact checkout's service sources and integration tests into the disposable NoCloud volume, compiles the two C binaries inside FreeBSD, logs file identity and SHA256, and runs the existing tests as an unprivileged account on an isolated session bus. Native acceptance is pending. The CI matrix separately repeats graphics (Wayland/Gamescope), upstream SteamOS Manager Rust audit, and inbound OpenSSH tests with the launcher proven by R05. No graphical compositor output, actual Steam client, or hardware integration is claimed. No Switch changes. Binary dependencies are not fully version-locked.

## R09: verified binary/package exports

Graphics mode now exports the source-built Wayland and Gamescope packages. Native mode exports a tar archive of the tested FreeBSD session binaries, preserving execute permissions. Both provide an installed package/origin manifest and guest SHA256 checksums. The host attaches a separate 128 MiB disposable FAT volume, extracts only the expected filenames after a successful guest shutdown and acceptance marker, validates the hashes independently, and publishes the outputs in the CI artifact's packages directory. No host directory or real console storage is mounted into the guest. The package manifest records installed dependencies, not a complete offline bundle; their binaries, redistributability audit, repository signing, dependency closure and installable system image are subsequent gates. R09 repeats only the native and graphics workloads; R08 retains the manager and SSH jobs. No successful artifact export asserted until CI evidence.

## R10: official track name and evidence graph

Owner terminology: EXODUS Tumor (Опухоль) is the architectural/source-port track. Frankenstein names a separate programming language. Existing branch names, source directory and workflow file paths remain historical identifiers to avoid breaking stacked PRs and in-flight checks. New branch names use tumor. The integration graph now records scoped CI evidence for FreeBSD QEMU boot and Linux service/D-Bus tests; the graph validator checks unique nodes, dependency references, cycles and evidence fields. This is not proof of native FreeBSD application builds or physical Switch support. R08/R09 native jobs were still pending at inspection.

## R12: fix nuageinit PATH and accept native inbound SSH

R08 graphics job 138808871064 failed after successfully installing git-lite, with /var/cache/nuageinit/user_data: git: not found and exit 127. Explicitly include FreeBSD /usr/local/bin and /usr/local/sbin in all package-dependent guest workloads, and check command availability before building. Re-run graphics, manager and native export modes; no successful build claimed until new evidence.

R08 SSH job 138808871803 in run 45496663587 passed EXODUS_SSH_AUTH_PASS, EXODUS_SESSION_INTENT_WITHOUT_REBOOT_PASS, EXODUS_SSH_REVOCATION_PASS, EXODUS_SSH_REFERENCE_PASS, guest exit 0. This establishes native base OpenSSH server on QEMU FreeBSD ARM64 with loopback tests, not external Switch connectivity or actual graphical mode switching. R10 host graph validation completed successfully on both architectures. No console changes.

## R13: Linux ARM64 ABI and Cortex-A57 instruction admission

The official FreeBSD handbook documents Linuxulator on AArch64 (https://docs.freebsd.org/en/books/handbook/linuxemu/); FreeBSD 15.1's rc.d/linux loads linux64 on aarch64. Valve issue https://github.com/ValveSoftware/steam-for-linux/issues/13288 reports SIGILL on Cortex-A57 in newer native ARM64 Steam builds and a suspected ARMv8.1 LSE requirement. These are distinct ABI and ISA gates.

The new linuxabi workload compiles two original static Linux AArch64 ELF probes on the ARM64 Linux host: a baseline ARMv8.0 write/exit syscall program and a variant using LDADD. It copies them into the existing disposable Cortex-A57 FreeBSD guest, enables the supported Linux ABI using service linux onestart, brands the ELF files, and tests execution as an unprivileged account. Baseline success and expected SIGILL (132) for the LSE variant are required. ELF headers and serial evidence are exported. No external Linux rootfs, Steam binaries, accounts, or physical console changes. This is not a proof of Steam compatibility, graphics, glibc, or translation of unsupported instructions. R12 remains responsible for pending full native/graphics/manager workloads; R13 CI runs only the short ABI gate to limit duplicate resource use.

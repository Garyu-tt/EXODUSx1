# EXODUS Plasma: native desktop on Horizon

Status: proposed architecture; owner-directed goal. No working port claimed.

## Product goal
Provide a Steam Deck-like dual session on Switch: controller-first Gaming Mode and a real KDE Plasma Desktop Mode, not a screenshot imitation or remote desktop. Retain Console and Headless modes where feasible. Use native Horizon kernel and EXODUS user-space contracts, not a Linux boot dependency. Gaming shell (#18) must remain usable if the desktop port is blocked.

## Porting graph
Horizon / libnx -> EXODUS HAL + NX-POSIX (#19) -> graphics/input/audio/storage/IPC adapters -> Qt Core -> Qt GUI/QPA + Qt Widgets/QML -> D-Bus transport + KDE Frameworks -> compositor/Wayland/KWin backend -> Plasma, Dolphin, Konsole, Kate.

Evaluate Mesa Switch, EGL/Vulkan and deko3d against pinned source revisions and device support. Qt's platform abstraction is a candidate; do not assume that a QPA plugin alone makes KWin portable. Inventory Linux-specific DRM/KMS, libinput, udev, process and shared-memory assumptions. Evaluate Wayland transport and an EXODUS-native display backend before selecting the final compositor architecture.

## Milestones and acceptance gates
- [ ] Audit Qt 6 build dependencies, C++ toolchain, TLS, filesystem, threads, clocks, sockets, event loop and dynamic loading. Pin sources and review licenses.
- [ ] Build minimal Qt Core console binary and verify timers, file IO and threads on hardware.
- [ ] Prototype Horizon QPA: window/surface lifecycle, EGL or Vulkan rendering, fonts, DPI, touch, keyboard and Joy-Con input; demonstrate a native Qt window.
- [ ] Qt Widgets and Qt Quick/QML: text rendering, scrolling, focus, animation and measured frame pacing.
- [ ] Implement required process and IPC semantics; D-Bus session bus or compatible transport; KDE Frameworks minimal dependency closure.
- [ ] Konsole with real shell/PTY-like semantics, Kate editor, Dolphin with VFS. Clearly report unsupported features.
- [ ] Prototype compositor and Wayland/KWin backend, multi-window focus and input, clipboard, display scaling; compare alternatives where KWin cannot yet run.
- [ ] Launch Plasma Desktop, panel, app launcher and settings. No claim of full Plasma until real components execute locally.
- [ ] Add Gaming/Desktop session switching, dock/undock resolution changes, suspend/resume, controller navigation, accessible keyboard input and recovery to gaming shell.
- [ ] Measure peak/idle RAM, frame times, startup time, battery and thermals on real Switch; choose memory budget based on measurements; retain rollback.

## Out of scope for initial proof
SteamOS distribution port, proprietary Steam client availability, Linux kernel emulation and full Windows compatibility are separate tracks. Do not introduce unverified GPU drivers or mandatory desktop dependencies into boot. Bigscreen is an optional alternative interface, not a prerequisite.

## QA
Host tests for platform contracts; devkitA64 cross-build and native packaging; physical-device tests with evidence logs. Each milestone is independently reviewable, with explicit unsupported functions and no fake hardware results. Keep all early device tests reversible.

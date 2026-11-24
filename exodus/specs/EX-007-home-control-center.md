# EX-007: Exodus Control Center

Status: proposed, P0 shell usability. Actual HOME integration is mandatory; no implementation claimed.

## Product
Replace the actual long-press HOME quick settings panel on EXODUSx1 (not just add another overlay hotkey). Preserve ordinary short HOME, system sleep, brightness, audio and airplane mode. Open from both Nintendo and Windows games without terminating them, close with B or HOME, restore focus/input correctly. Support handheld, docked and touch/controller navigation. Preserve original system menu fallback and stock-mode behavior.

## Pages
- Quick: brightness/auto-brightness, volume/mute, audio output/Bluetooth where supported, Wi-Fi, airplane mode, sleep, battery/time; customizable favorite toggles.
- Performance: per-title Eco/Balanced/Performance/Custom presets, FPS cap and refresh (where supported), CPU/GPU/RAM readouts, thermal/power limits, optional on-screen FPS/frametime/1% low, temperatures, clocks and memory. Reuse 4IFIR/sys-clk, FPSLocker and Status Monitor; do not enable unsafe voltage/frequency changes by default. Unsupported controls disabled with reason.
- Windows runtime: active title status, per-game FEX/Box64 and graphics backend options (apply on restart where necessary), resolution/upscaling, controller/mouse mapping, on-screen keyboard, log export, graceful exit; no controls for unavailable capabilities.
- Steam: active downloads, progress, pause/resume, queue and notifications; account/sync status; open actual HOME store/library. Integrates EX-006, never claims unavailable features.
- Storage & connections: SD free space, MTP/FTP state, USB reconnect/restart when safe, explicit confirmation if active transfers may be interrupted; log export and optional remote access status.
- System: controller battery and mapping, screenshot/recording when supported, running title, crash diagnostics, optional module toggles with dependencies and safe rollback. No hidden network services or credential exposure.

## Implementation
1. Inventory Latitude/qLaunch source/licensing, HOS target versions and the genuine HOME-hold input/event path. Research Ultrahand/libtesla/ovlloader as reusable implementation substrate, but an alternate hotkey alone is not acceptance.
2. Implement UI/controller focus and state adapter to documented HOS services with a read-only fallback; bind real hardware/runtime capabilities. Budget memory and CPU; lazy-load nonessential tabs and stop telemetry polling while closed.
3. Integrate EPE performance profiles, ZibBurger telemetry when available, Windows and Steam runtime adapters without duplicate background daemons. Use authenticated local IPC with bounded messages; transactional settings, restore previous per-title profiles on exit and safe defaults on crash.
4. Validate on emuMMC, backup and reversible installation; ensure no simultaneous stock/new menu or input leakage. No proprietary redistribution without permission.

## Acceptance
- On real supported Switch build, short HOME is unchanged; long HOME opens actual new panel from HOME and at least one native and one supported Windows game. If runtime unavailable that gate remains pending. B/HOME returns focus without lost inputs or game termination.
- Brightness, sound, airplane mode, sleep and wake function; dock/handheld transition works. Stock fallback survives unsupported HOS, startup error and disabled module.
- Show measured baseline vs panel overhead (RAM/CPU/frametime), repeated open/close and sleep/wake; no persistent regression outside the panel. Document exact build IDs and real logs. Initial performance target, subject to measurement: open in under 500ms, no measurable persistent idle FPS loss.
- Real controls show confirmed state after changes, disabled unavailable features, no automatic unsafe overclocking, no destructive action without confirmation. Steam/Windows/USB dependent gates tracked separately.
- No completion on mock UI, alternate hotkey or PC-only tests.

References: https://github.com/ppkantorski/Ultrahand-Overlay ; https://github.com/masagrator/FPSLocker ; https://github.com/masagrator/Status-Monitor-Overlay ; https://github.com/rashevskyv/4IFIR/blob/main/docs/LATITUDE_UI.md

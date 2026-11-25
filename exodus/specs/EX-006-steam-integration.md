# EX-006: Native Steam client, storefront and unified HOME

Status: P0 product requirement; NOT delivered by offline importer PR #2.

## Non-negotiable product experience
On a Switch running EXODUSx1, with no PC or companion required, the user selects the former Nintendo eShop icon in the actual HOME shell, enters the Steam storefront, signs into their real Valve account (including Steam Guard), and sees their Steam library automatically synchronized into the same HOME library as native Switch titles. From HOME or the storefront they select an entitled game, download its supported Windows content directly to the console, install/update it and launch locally via the Windows runtime. Newly acquired games appear after sync; installed and uninstalled titles are distinguished. A standalone launcher, URL shortcut alone, PC manifest import, streaming or a JSON catalog alone does not satisfy EX-006. PR #2 is only an optional migration utility.

## Architecture and work packages
- [ ] Identify and obtain legally reusable Latitude/qLaunch source, locate eShop action and actual HOME title enumeration/render/launch paths. If unavailable, evaluate a compatible open-source replacement HOME; do not claim a shell hook without device evidence. Preserve native Nintendo entries and stock-mode rollback.
- [ ] Prefer integrating the official Steam client where technically and contractually supported, with a shell adapter exposing store, login, library, downloads and launch. Investigate installer drawing, unsupported OS, Wine CreateProcess/IPC and CEF; no full-client success claim before hardware tests.
- [ ] In parallel evaluate a native ARM64 Horizon client/service for supported Steam functionality if the official client cannot run: Steam store via Valve pages, legitimate authentication including Steam Guard, authenticated library/license metadata, game content and update delivery. SteamKit2 and DepotDownloader are reference candidates, not proven Horizon ports; evaluate licenses, platform support and Valve terms before reuse. OpenID identity alone does not authorize content downloads. Never bypass ownership, DRM or anti-cheat.
- [ ] Implement account/session lifecycle on the device; official login/checkout surfaces, scoped tokens where supported, secure storage, logout and expired/revoked-session handling; no passwords, tokens, or payment data in logs or repo.
- [ ] Unified catalog with provider + ID, artwork, owned vs installed vs downloading, compatibility, launch mode; sync at sign-in, on demand, after purchase and on reconnect. Cache per account; keep Nintendo data distinct and intact. Offline PC manifest import is optional only.
- [ ] Local download manager for entitled supported games: select Windows content, free-space checks, progress, pause/resume, integrity verification, install/update/remove, interruption recovery and safe paths. Keep licensing checks intact. If a legitimate distribution path is unavailable, show explicit unsupported state, not fake installation.
- [ ] Local runtime integration: per-title prefixes/config, launch from HOME, game exit back to HOME, logs, supported Steamworks integration only. No fabricated ownership or service responses. Optional streaming is separate, never the primary acceptance path.
- [ ] Device testing on emuMMC with backups; reversible shell changes, no Nintendo credential or entitlement modifications; no ban-prevention guarantee.

## Release acceptance (all required)
1. Start from actual HOME; former eShop entry opens Steam store, not a PC companion or separate manual tool.
2. Sign in using a real test account on Switch, including Steam Guard; reboot and session recovery or safe re-auth work; logout clears sensitive state.
3. Owned supported Steam games, including uninstalled titles, appear alongside Nintendo games in actual HOME without manual imports. A newly acquired or newly entitled game appears after sync. Native Nintendo launch remains working.
4. Select at least one legitimately entitled compatible Windows title, download from authorized Steam delivery directly on Switch, verify content, install it, and start it from HOME with no PC. Demonstrate interrupted download recovery and at least one update or integrity recheck. Record actual hardware logs and exact build IDs.
5. Unsupported titles and unavailable backend operations are clearly indicated; no DRM/entitlement bypass. Rollback to original shell tested.
6. No completion based solely on PC tests, mocks, screenshots, a web shortcut, offline importer or partial backend. Report each gate independently.

## Dependencies and risks
EX-001 GTA San Andreas hang; EX-002 broken Steam installer rendering; EX-003 Steam unsupported-system message; EX-004 intermittent USB/MTP; EX-005 memory compression. Actual backend in installed 4IFIR still unconfirmed. Autorun/Wine-NX is a candidate with documented single-process limits. Latitude source availability, Horizon runtime compatibility, legitimate Steam client/content access and on-device session storage remain open. Work in parallel, but do not relabel a subset as the finished integration.

## References
- https://github.com/Garyu-tt/EXODUSx1/pull/2
- https://github.com/rashevskyv/4IFIR/blob/main/docs/LATITUDE_UI.md
- https://github.com/autorunhq/autorun
- https://partner.steamgames.com/doc/features/auth
- https://github.com/SteamRE/SteamKit
- https://github.com/SteamRE/DepotDownloader
- https://store.steampowered.com/

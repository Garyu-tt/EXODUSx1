# EX-006: Native Steam integration

Status: proposed, P0 for shell/library MVP; runtime compatibility remains unverified.

## Goal
Replace the Nintendo eShop shortcut in the custom HOME shell with a configurable Steam storefront. Merge Nintendo titles, Steam library entries, local Windows apps and homebrew into one controller-friendly library without changing Nintendo entitlement data. Unofficial integration; preserve rollback and original behavior outside the custom environment.

## Work packages
- [ ] Inventory actual 4IFIR/Latitude/qLaunch source availability, license and build; locate eShop shortcut routing and verify installed Windows runtime identity.
- [ ] Store entry opens official Steam storefront. Checkout and credentials stay with Valve; if Horizon webview cannot support it, hand off to trusted browser/companion. Never store passwords or payment data.
- [ ] Link Steam account with official OpenID. Obtain permitted owned-game metadata with privacy-aware API access or import user-owned PC manifests. Cache metadata offline, support logout. OpenID alone is not library authorization.
- [ ] Unified catalog schema: provider, provider ID, name, artwork, installation state, compatibility state, launch mode, last played. Providers: Horizon native, Steam, local Windows, homebrew. Avoid duplicate IDs and distinguish licensing per platform.
- [ ] Download/install/update via legitimate Steam distribution or authorized user imports where supported; progress, cancellation, disk checks. No DRM, ownership or anti-cheat bypass; metadata alone is not entitlement.
- [ ] Local runtime: investigate Steam installer rendering, unsupported OS message, process spawning, IPC, CEF; full Steam client support is a separate milestone. Candidate Autorun/Wine-NX currently documents single-process limitation.
- [ ] Optional Moonlight/Sunshine remote launch, explicitly distinguished from local execution; no PC required for supported local games.
- [ ] Cloud saves and achievements only through supported authorized interfaces; no fabricated Steamworks responses.
- [ ] Test on emuMMC, back up, maintain rollback; no guarantee against console bans.

## Acceptance gates
1. HOME Steam entry opens official store, back works, original shortcut is restorable.
2. Linked test account displays permitted library entries; unavailable/private data clearly reported; logout clears sensitive cached state.
3. Nintendo and Steam entries coexist, cache works offline, compatibility/installation/launch mode visible.
4. At least one legitimately owned compatible Windows title launches locally; optional separate remote test. Logs and performance evidence required before claiming runtime compatibility.
5. No credentials or proprietary assets committed without permission; no changes to Nintendo service auth or entitlement.

## Dependencies and observed regressions
EX-001 GTA San Andreas hang after logo; EX-002 broken Steam installer rendering; EX-003 Steam client unsupported-system message; EX-004 intermittent USB/MTP; EX-005 experimental memory compression. Actual causes not established. Latitude source provenance and current binary inventory are open blockers. Do not block storefront/library MVP on full client support.

## References
- https://github.com/rashevskyv/4IFIR/blob/main/docs/LATITUDE_UI.md
- https://github.com/autorunhq/autorun
- https://partner.steamgames.com/doc/features/auth
- https://store.steampowered.com/

# EX-015: Exodus Shell / native HOME redesign

Priority: P0 for R0.1 usability. Owner requests replacement of the currently selected interface, interpreted as the 4IFIR Latitude HOME shell. This is a new system shell requirement, not merely a theme or an additional homebrew launcher. Visual direction is proposed, not owner-approved.

## Product requirements
- Replace or substantially rework Latitude UI itself; retain the ability to revert to the original compatible shell. Inventory exact installed HOS, shell identity, source availability, licenses and service contracts before implementing.
- Unified actual HOME library for Nintendo titles, authorized Steam/Windows titles and homebrew; search, filters, sorting, favorites, recent games, installed/download state and useful game details. EX-006 (PR #3) owns Steam authentication, store and authorized direct downloads.
- Replace the default store shortcut with Steam Store; integrate Homebrew Store and file manager as discoverable actions rather than hidden button combinations. No PC requirement for core functionality.
- Integrate EX-007 (PR #4) actual long-HOME Control Center, EX-004 USB status, EX-008 performance profiles and EX-011 release/recovery status. Preserve ordinary HOME behavior, resume, sleep, controller pairing, users, saves, album and system settings.
- Consistent gamepad-first navigation, predictable focus, B/back, readable typography at 720p handheld and 1080p docked, touch optional, useful loading/error/offline/empty states. Proposed visual direction: Steam Deck-like information hierarchy with restrained Switch-style controls, dark/light themes and configurable grid/list. Avoid mandatory animated backgrounds, blur or heavy media.
- Decouple visual shell from provider and runtime services. Lazy-load artwork, bound caches and memory, stop unnecessary background work when a game runs. No credentials or private logs in UI.
- Reuse existing open-source components only after licensing/provenance review; do not assume Latitude source is available. Evaluate a native replacement if patching it is infeasible. No unsupported redistribution.

## Acceptance
1. Actual system HOME is replaced on a compatible real Switch emuMMC; standalone NRO or skin alone is insufficient. Document boot/start/resume and reversible installation. Preserve a working fallback.
2. Controller-only navigation covers all primary flows; no dead ends or focus leakage. Test handheld/docked transitions, sleep/wake, offline and recovery.
3. Launch and return from native titles and supported Windows titles, maintain correct library status and existing saves. Steam and Control Center dependencies have separate real-device acceptance gates; unavailable features are visibly disabled.
4. Compare HOME opening time, memory footprint, animation frame time and gameplay overhead against the existing shell; no significant regression without explicit review. Record actual hardware evidence.
5. Final visual direction and interaction design require owner review before default rollout.

This PR is a specification only. No shell code, screenshots or hardware tests are claimed. Exact Latitude binary/source provenance remains unresolved.

# EXODUS standalone CorkDeck UI R02

This is an experimental host build target for #18/#40, not a HOME replacement or a Switch binary. It links the original CorkDeck drawing, audio, SVG and input primitives, without linking launcher.c, shell.inc, Steam, FEX, Box64 or Wine. The settings preview uses the donor's real grouped settings widget rather than reproducing it.

## Reproduce

Requires a C compiler, git, pkg-config, SDL2 (2.0.18 or later), SDL2_ttf, and a user-provided TTF font. From EXODUS root:

```sh
git clone https://github.com/Garyu-tt/CorkDeck.git
git -C CorkDeck checkout --detach adc0b52284e9e57d18808427a770a2a162c76bf0
sh exodus/home/corkdeck/build-host.sh CorkDeck /tmp/exodus-settings
/tmp/exodus-settings /path/to/font.ttf
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy /tmp/exodus-settings /path/to/font.ttf --smoke
```

The smoke mode draws one frame and exits; it is not an input or hardware test. The normal mode uses donor focus navigation and grouped rows. Platform and battery are reported by SDL when available, not by a simulated Switch. On host, Horizon-only settings explicitly report unavailable. The native adapter is selected by __SWITCH__; there are no writes.

The donor is pinned and referenced, not copied into EXODUS. Preserve CorkDeck/Wine LGPL notices and third-party notices; review per-file provenance (including the SVG and focus-border implementation) before shipping linked binaries. No distribution of donor assets or binaries is part of this change.

## Gates

- Compile and run the host smoke and interactive settings screen.
- Integrate catalog provider adapter (#46) and native title inventory (#15).
- Build separately with devkitPro and Switch SDL2; validate applet/service permissions and hardware status.
- Keep Latitude/uLaunch fallback until device acceptance.
- Extract further actual CorkDeck shell screens incrementally; shell.inc currently relies on launcher state and is not standalone.

No successful build or device run is claimed by this PR.

## R03: Horizon status provider

The adapter uses PSM (battery/charger), NIFM (wireless flag and connection status), set:sys (firmware version and Bluetooth flag), LBL (configured brightness) and APM (performance mode). API signatures were inspected at libnx commit b156b4774fb2df7b93402169509266da7547fc4c. Each service has an independent initialization/exit and errors are displayed as unavailable, including the Result code where applicable. NIFM user and set:sys permissions may differ by applet and firmware; an unavailable read does not imply a disabled feature. Bluetooth is an enable flag, not pairing state. The APM mode is not a clock measurement. Snapshot refresh occurs when the settings view is entered or re-entered, not continuously inside the donor widget.

Host fallback check, requiring only a C compiler:

```sh
cc -std=c11 -Wall -Wextra -Werror exodus/home/corkdeck/test_horizon_settings.c -o /tmp/exodus-settings-check
/tmp/exodus-settings-check
```

Native compilation and hardware permission/firmware checks are still required. In particular, do not infer HOS 4.x compatibility from the version of the pinned headers. No settings setters or firmware changes are included.

## R04: unified library preview

The host build now additionally needs C++17 and nlohmann/json headers (set JSON_INCLUDE to the directory containing json.hpp if not /usr/include/nlohmann). The existing catalog parser is shared rather than reimplemented. Open a version 1 EXODUS catalog snapshot:

```sh
/tmp/exodus-settings /path/to/font.ttf --library /path/to/exodus-catalog.json
```

The library uses the original CorkDeck drawing/input primitives with a new eight-card paginated view. Directional controls select cards, L/R change pages, A shows metadata, B exits. It recognizes Switch, Homebrew, Steam, CorkDeck and Other. Covers, live scanning, touch-card selection, and provider-specific launch dispatch are not implemented in R04. No catalog input is executed.

Bridge test (use a disposable file path):

```sh
c++ -std=c++17 -Wall -Wextra -Werror -I "${JSON_INCLUDE:-/usr/include/nlohmann}" \\
  exodus/home/corkdeck/test_library_catalog.cpp \\
  exodus/home/corkdeck/library_catalog.cpp exodus/home/ulaunch/catalog.cpp \\
  -o /tmp/exodus-catalog-check
scratch=$(mktemp)
/tmp/exodus-catalog-check "$scratch"
rm -f "$scratch"
```

The shared parser is GPL-2.0-only; verify combined donor licensing and notices before distribution. Build/tests and native execution have not been verified in this session.

## R05: explicit provider launch routing

Native Switch title IDs are 16 hexadecimal characters (nonzero) and must appear in a fresh NS application inventory before appletRequestLaunchApplication. Library applet launches require HOS 5.0 or later; unsupported contexts fail closed. Homebrew IDs are relative to sdmc:/switch, must use .nro, cannot traverse directories, and are checked for a regular file and NRO0 header before envSetNextLoad. A supported loader must be present. The user must confirm the target; accepted requests exit the preview normally. This is a request handoff, not proof that the title executed. Steam and CorkDeck are recognized routes but have no active backend yet; the host never launches anything. No arbitrary command strings are executed.

Portable routing test (no SDL or libnx needed):

```sh
cc -std=c11 -Wall -Wextra -Werror exodus/home/corkdeck/test_launch_route.c -o /tmp/exodus-route-check
/tmp/exodus-route-check
```

Actual HOS launch handoff, permissions, return-to-shell behavior and NRO argument handling require a native build and physical device tests. Use emuMMC and retain the original HOME. No system menu replacement is installed by this change.

## R06: installed Windows runtime and Steam library handoff

The pinned CorkDeck runtime.c at adc0b52284e9e57d18808427a770a2a162c76bf0 accepts argv[1] as the selected executable. R06 uses the existing libnx next-load mechanism and fixed runtime path sdmc:/switch/wine/wine-nx-runtime.nro. It shares the exact NRO structural preflight sources from PR #11 (link exodus/runtime/autorun_probe.c in native builds). The caller confirms the target and must be in full application mode. Windows targets are bounded SD paths with .exe extension, no traversal, no embedded quotes or controls, and a readable MZ header. USB and other mounts are not supported yet. No shell or command interpreter is invoked. The MZ check is a preliminary file sanity check, not a full PE validation or compatibility guarantee.

Steam entries validate the numeric AppID but open the installed CorkDeck library without passing an app ID: the user selects the game there. This is not direct Steam game launch or ownership verification. The Steam snapshot/sync work in #44 is separate. A pending run-next.txt blocks both routes, because the donor prioritizes queued jobs over CLI targets. The guard cannot eliminate races with another writer. NRO structural validation does not authenticate the installed runtime or identify its version; use a compatible CorkDeck build. Return to EXODUS, low-address requirements and actual game launch need device tests.

Portable argument test:

```sh
cc -std=c11 -Wall -Wextra -Werror exodus/home/corkdeck/test_runtime_handoff.c -o /tmp/exodus-runtime-check
/tmp/exodus-runtime-check
```

Native compilation, loader buffer limits on the installed loader, and physical-device handoff remain unverified. No new runtime is installed and no system firmware or HOME changes are made.

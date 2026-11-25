# EXODUS Shell

The standalone shell builds as an SDL host preview and a Switch NRO. It uses a pinned CorkDeck subset for widgets, input, audio and images, with an EXODUS focus frame. The settings preview uses CorkDeck's grouped settings widget.

## Reproduce

Requires C and C++ compilers, git, Python 3, pkg-config, SDL2 (2.0.18 or later), SDL2_ttf, libpng, TurboJPEG, nlohmann/json headers and a user-provided TTF font. From EXODUS root:

```sh
git clone https://github.com/Garyu-tt/CorkDeck.git
git -C CorkDeck checkout --detach adc0b52284e9e57d18808427a770a2a162c76bf0
sh exodus/home/corkdeck/build-host.sh CorkDeck /tmp/exodus-settings
/tmp/exodus-settings /path/to/font.ttf
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy /tmp/exodus-settings /path/to/font.ttf --smoke
```

The smoke mode draws one frame and exits; it is not an input or hardware test. The normal mode uses donor focus navigation and grouped rows. Platform and battery are reported by SDL when available, not by a simulated Switch. On host, Horizon-only settings explicitly report unavailable. The native adapter is selected by __SWITCH__; there are no writes.

The [release source and license notes](release-source.md) identify the prepared donor source, the EXODUS license and the pinned build environment.

## Gates

- Compile and run the host smoke and interactive settings screen.
- Integrate catalog provider adapter (#46) and native title inventory (#15).
- Build separately with devkitPro and Switch SDL2; validate applet/service permissions and hardware status.
- Keep Latitude/uLaunch fallback until device acceptance.
- Extract further actual CorkDeck shell screens incrementally; shell.inc currently relies on launcher state and is not standalone.

The CorkDeck CI workflow covers the host build and one-frame smoke. Switch device acceptance is tracked separately.

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
c++ -std=c++17 -Wall -Wextra -Werror -I "${JSON_INCLUDE:-/usr/include/nlohmann}" \
  exodus/home/corkdeck/test_library_catalog.cpp \
  exodus/home/corkdeck/library_catalog.cpp exodus/home/ulaunch/catalog.cpp \
  -o /tmp/exodus-catalog-check
scratch=$(mktemp)
/tmp/exodus-catalog-check "$scratch"
rm -f "$scratch"
```

The shared parser carries GPL-2.0-only. Its source and license accompany the shell release.

## R05: explicit provider launch routing

Native Switch title IDs are 16 hexadecimal characters (nonzero) and must appear in a fresh NS application inventory before appletRequestLaunchApplication. Library applet launches require HOS 5.0 or later; unsupported contexts fail closed. Homebrew IDs are relative to sdmc:/switch, must use .nro, cannot traverse directories, and must pass the shared structural NRO preflight before envSetNextLoad. A supported loader must be present. The user must confirm the target; accepted requests exit the preview normally. This is a request handoff, not proof that the title executed. Steam and CorkDeck are recognized routes but have no active backend yet; the host never launches anything. No arbitrary command strings are executed.

Portable routing test (no SDL or libnx needed):

```sh
cc -std=c11 -Wall -Wextra -Werror exodus/home/corkdeck/test_launch_route.c -o /tmp/exodus-route-check
/tmp/exodus-route-check
```

Actual HOS launch handoff, permissions, return-to-shell behavior and NRO argument handling require a native build and physical device tests. Use emuMMC and retain the original HOME. No system menu replacement is installed by this change.

## R06: installed Windows runtime and Steam library handoff

The pinned CorkDeck runtime.c at adc0b52284e9e57d18808427a770a2a162c76bf0 accepts argv[1] as the selected executable. R06 uses the existing libnx next-load mechanism and fixed runtime path sdmc:/switch/wine/wine-nx-runtime.nro. It shares the exact NRO structural preflight sources from PR #11 (link exodus/runtime/autorun_probe.c in native builds). The caller confirms the target and must be in full application mode. Windows targets are bounded SD paths with .exe extension and without traversal, embedded quotes or controls. The shell checks the PE executable headers and on-disk section ranges before handing off. Unsupported machine and format combinations leave the shell open with a separate error. Eligible combinations are x86 PE32 and x64 or ARM64 PE32+, matching the pinned CorkDeck build family. x86 and x64 still require the corresponding translator support in the installed runtime. USB and other mounts are not supported yet. The handoff does not start a command interpreter.

Steam entries validate the numeric AppID but open the installed CorkDeck library without passing an app ID: the user selects the game there. This is not direct Steam game launch or ownership verification. The Steam snapshot/sync work in #44 is separate. A pending run-next.txt blocks both routes, because the donor prioritizes queued jobs over CLI targets. The guard cannot eliminate races with another writer. NRO structural validation does not authenticate the installed runtime or identify its version; use a compatible CorkDeck build. Return to EXODUS, low-address requirements and actual game launch need device tests.

Run the portable argument and PE preflight tests.

```sh
cc -std=c11 -Wall -Wextra -Werror exodus/home/corkdeck/test_runtime_handoff.c -o /tmp/exodus-runtime-check
/tmp/exodus-runtime-check
cc -std=c11 -Wall -Wextra -Werror exodus/home/corkdeck/test_windows_pe.c exodus/home/corkdeck/windows_pe.c -o /tmp/exodus-pe-check
/tmp/exodus-pe-check
```

Native compilation, loader buffer limits on the installed loader, and physical-device handoff remain unverified. No new runtime is installed and no system firmware or HOME changes are made.

## R07: offline artwork and library filters

Y cycles All, Switch, Homebrew, Steam, CorkDeck, Other, Favorites and Recently played. X toggles a session-only favorite. Recent uses the snapshot's nonzero last_played values, descending; it is not a log of confirmed launches. Favorites are not persisted. Empty filters stay navigable. The selected filtered row always resolves to the original catalog index before launch confirmation.

The shared parser retains artwork paths and last_played. The original CorkDeck launcher_image.c decoder handles PNG/JPEG files in sdmc:/switch/wine/ or sdmc:/ulaunch/ only. No URL fetch, arbitrary paths or SteamGridDB credentials are used. Missing/invalid art leaves a text card. Only eight visible thumbnails are kept, scaled to at most 256 pixels per side (at most 2 MiB RGBA pixel payload excluding driver overhead). Decoding remains synchronous on page changes, so device latency and transient allocations still need measurement. The host build additionally requires pkg-config packages libpng and libturbojpeg.

SteamGridDB's donor API was audited; its network fetch is deliberately deferred until an explicit download action, key storage policy and offline fallback are implemented. No artwork is bundled.

Portable filter test:

```sh
cc -std=c11 -Wall -Wextra -Werror exodus/home/corkdeck/test_library_filter.c -o /tmp/exodus-filter-check
/tmp/exodus-filter-check
```

Native build, interaction, rendering and performance acceptance remain pending.

## R08: unified shell

The default view is now Gaming Mode home. Up/down moves among Library, Settings, Terminal, SSH and Exit; A opens, B returns/exits. Library and settings are internal views. A successful launch handoff leaves the shell rather than returning to its menu. Missing tools report an error without scheduling a launch.

Host: /tmp/exodus-settings FONT.ttf --shell CATALOG.json (or FONT.ttf alone with the default catalog path). The explicit --library mode remains available. --smoke draws the home screen. Native no-argument startup expects a user-supplied font at sdmc:/switch/exodus-shell/font.ttf and catalog at sdmc:/ulaunch/exodus-catalog.json. The standalone NRO build is documented under R10.

The Terminal and SSH tools use fixed local paths sdmc:/switch/exodus-terminal/exodus-terminal.nro and sdmc:/switch/exodus-ssh/exodus-ssh.nro. These are packaging conventions, not autodiscovery. Install the corresponding independently built tools (#43 and #37). The UI requires full application mode, a compatible loader, successful NRO preflight and explicit confirmation. No terminal or SSH backend is duplicated. Tool return-to-shell is not implemented; Homebrew loader behavior determines the destination. No Nintendo eShop, account actions, or stock HOME replacement is exposed by this menu. This does not guarantee freedom from platform enforcement.

Portable navigation test:

```sh
cc -std=c11 -Wall -Wextra -Werror exodus/home/corkdeck/test_shell_menu.c -o /tmp/exodus-menu-check
/tmp/exodus-menu-check
clang -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined exodus/home/corkdeck/test_shell_menu.c -o /tmp/exodus-menu-sanitize
/tmp/exodus-menu-sanitize
```

Both passed locally against files whose git blob hashes matched the branch: shell_menu.h 6996d43f37a57dfdf1a2ed44dbc2c1a550f7acfa and test_shell_menu.c 4f253eb757e46c6a5a6738cb20281d2e0b91857e. SDL and native compilation, actual screen navigation, tool handoff and device acceptance remain pending.

## R09: Control Center preview

Plus opens the quick panel from Gaming Mode home. X refreshes the snapshot; B or Plus returns. Ten fields report battery, network, Wi-Fi, Bluetooth, brightness, APM mode, process memory used/available budget (not total console RAM), SD used/total capacity, SoC and board temperatures. Uses existing settings adapter plus svcGetInfo, FS and TS service read calls. Legacy temperature reads before HOS 17 and session calls on HOS 17+ follow pinned libnx source. Failed or unsupported reads display unavailable, not fabricated readings. No polling while the panel is idle, no clock, fan, voltage, radio or brightness writes, no Nintendo network actions. No HOME button interception yet; this is the in-shell preview for #4.

Portable capacity arithmetic and unavailable fallback test passed GCC and Clang ASan/UBSan locally, with the two test/source files reconstructed and blob-checked; horizon_settings.h was stubbed for the host-only build (no native symbols are used). Full SDL and Switch SDK compilation and service permissions on real firmware remain unverified.


## R10: standalone native build and HOME admission

Build using devkitPro with devkitA64, libnx, Switch SDL2/SDL2_ttf, PNG and TurboJPEG portlibs, Python 3 and the nlohmann/json headers. Pass the exact donor checkout or the prepared donor tree from the source archive.

```sh
sh exodus/home/corkdeck/build-switch.sh /path/to/CorkDeck /tmp/exodus-native
cc -std=c11 -Wall -Wextra -Werror exodus/home/corkdeck/check_shell_nro.c exodus/runtime/autorun_probe.c -o /tmp/check-shell-nro
/tmp/check-shell-nro /tmp/exodus-native/exodus-shell.nro
```

The script restricts pkg-config to the Switch portlibs. Set JSON_INCLUDE if json.hpp is elsewhere. CI pins the donor, the native image and the JSON headers. It checks the binary and compares independent builds. The [release source notes](release-source.md) document the build inputs.

### Safe first deployment

After successful compilation and preflight, copy only exodus-shell.nro into a new SD directory /switch/exodus-shell/. Place your own licensed TTF font as font.ttf in that directory. Use an existing EXODUS catalog at /ulaunch/exodus-catalog.json. Open the ordinary NRO from the existing Homebrew Menu (or an existing launcher that supports NROs). Do not install a title forwarder, overwrite qLaunch/uLaunch/Latitude files, or hook HOME at this stage.

Rollback: exit the NRO and use the original menu. Removing the dedicated NRO directory removes this optional shell; do not delete the shared catalog, runtime or existing HOME files. Before considering HOME replacement, require on-device checks for startup, failure reporting, controls, handheld/dock, suspend/resume, memory budget, all handoffs, and return/exit. Only then implement a separately reviewed HOME adapter with an independently tested recovery path. HOS 4.9/older firmware support is not established.

## R11: first physical hardware checkpoint (2026-11-24)

Operator photos of Switch Lite running the R10 binary (SHA256 29696de61b56067a463ba8f651e6480ff62f7ba5c576fe30eebea9ae5c40fca3) show the home screen and Control Center. Confirmed visually: startup, SDL rendering, TTF text, transition via Plus, and ten populated telemetry fields. Observed snapshot: battery 90% external power, Wi-Fi enabled and connected, Bluetooth enabled, brightness 68%, APM Normal, kernel memory charge 3089 / 3189 MiB, SD used 12845 / 59668 MiB, SoC 28.2 C, board 27.8 C. Photos are operator evidence, not automated instrumentation or a formal benchmark. Firmware version is not established by these photos.

The kernel memory charge is not a measurement of live EXODUS allocations. libnx heap reservation may contribute; do not infer a memory leak or claim allocator usage from these numbers. R11 relabels the field; separate live allocation measurements are pending.

R11 also replaces the visible donor product label on the shell-owned headers and prefixes footer action labels with ASCII controller names. This is a fallback for blank glyphs seen on hardware, not a fix to the donor glyph renderer or internal settings/dialog headers. Touch hitboxes remain managed by the original UI. Donor source and licensing are retained. Native compilation and on-device acceptance of R11 are pending. R10 is not a HOME replacement. Library, launch handoffs, settings, exit and suspend/resume still require separate tests.



## R12: physical acceptance follow-up and local inventory

Operator photos confirm R10 on Switch Lite running firmware reported as 20.5.0: Settings System, Connectivity and Hardware panels rendered; Library reports Catalog unavailable; Terminal and SSH display the expected missing/invalid NRO dialog. Native platform text is now explicit rather than SDL's unknown string.

When the default SD catalog is unavailable, the native shell attempts a read-only live inventory of registered application IDs and localized names via NS (adapted from #15) and scans /switch plus one subdirectory level for structurally valid NRO files. The scan caps records at 4096 and directory entries visited at 4096. It skips the shell's own standard path. A failed NS scan discards partial NS records; discovered homebrew may still be shown. A valid existing catalog remains authoritative; an invalid catalog is not silently replaced. No export, network request, file deletion, or automatic launch occurs. The live list does not yet include artwork, Steam sync, or CorkDeck Windows catalog import. On-device service permissions, scan completeness, latency, and launch handoffs remain to be tested.

The workflow also builds two independent optional NRO tools from fixed revisions of #43 (terminal) and #37 (SSH), packages them under the shell's expected SD paths, and publishes each artifact only after structural validation. These are the current terminal and one-shot SSH implementations, not a complete FreeBSD userspace or an interactive SSH PTY. Build results and hardware execution must be checked separately. Do not install unverified artifacts.

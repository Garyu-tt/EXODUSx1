# EXODUS library in uLaunch HOME

The adapter adds **EXODUS library** to the Quick Menu in uLaunch's `uMenu` HOME interface. From the main uMenu screen, open Quick Menu with **ZL** or **ZR** and select **EXODUS library**. The dialog displays application names and providers in pages of six. **Previous**, **Next** and **Close** use uMenu's controller dialog.

The viewer opens `/ulaunch/exodus-catalog.json` on the SD card when selected. Use a version 1 snapshot from the EXODUS Steam importer, or combine it with system and homebrew inventory snapshots using the EXODUS catalog merger. The viewer accepts up to 2 MiB and 4,096 entries. It requires the eight version 1 entry fields, preserves order, rejects duplicate identities and opens an error dialog for unreadable or malformed snapshots. Display names are shortened to fit. The catalog carries snapshot metadata on the console. Live ownership, downloads and launch routing are separate work. [Catalog merger and Switch exporter](https://github.com/Garyu-tt/EXODUSx1/pull/15) · [Homebrew inventory](https://github.com/Garyu-tt/EXODUSx1/pull/13) · [EX-006](https://github.com/Garyu-tt/EXODUSx1/pull/3)

The viewer accepts UTF-8 catalogs and BOM-marked UTF-16LE catalogs, including snapshots saved by redirecting importer output in Windows PowerShell 5.1.

## Automatic Steam refresh

Use the patched `uMenu` together with the current EXODUS Steam Sync NRO. Each successful sync publishes a Steam-only snapshot in `/switch/exodus-steam-sync/snapshots/` after validation and flushing the staged file. Reopen **EXODUS library** to read the latest usable Steam library and the current `/ulaunch/exodus-catalog.json`. Synced Steam entries appear first, followed by the existing non-Steam entries in their original order. A synced empty Steam library removes the Steam entries from the view. [Steam Sync build and sign-in](../../steam_switch/README.md)

The viewer skips incomplete, malformed and oversized Steam snapshots, as well as snapshots that would exceed the 4,096-entry combined limit. With no usable Steam snapshot it displays the manual catalog. With no manual catalog it can display the Steam snapshot by itself. An existing invalid manual catalog still shows its catalog error. The NRO publishes into its own directory and keeps the latest eight valid generations when cleanup succeeds.

To restore the manual catalog view, close the NRO and move the `snapshots` directory to a backup location. Both apps leave `/ulaunch/exodus-catalog.json` and earlier manually exported catalogs intact. A later successful sync creates a new snapshot directory.

## Check a Steam game's launch status

On a page with Steam games that have a valid AppID, select **Steam status** and choose a game. The dialog shows its Steam AppID and whether the row came from the saved Steam Sync library or the manual catalog.

uMenu sends the AppID to the matching uSystem through read-only commands. uSystem checks `/switch/exodus-steam/steamapps/appmanifest_<AppID>.acf` on the SD card. It matches the manifest AppID and checks for the declared game folder under `steamapps/common/` when `StateFlags` has the FullyInstalled bit set. The dialog reports absent, invalid or incomplete manifests, missing game folders and matching manifest and folder evidence. It displays a separate launch status. [Steam library layout](https://help.steampowered.com/en/faqs/view/4BD4-4528-6B2E-8327)

A matching manifest and folder establish local disk evidence only. Actual launch requires an approved local Switch runtime, verification of installed game files and an official Steam entitlement path. [Steamworks installation and entitlement](https://partner.steamgames.com/doc/api/ISteamApps#BIsAppInstalled) · [EX-006 tracker](https://github.com/Garyu-tt/EXODUSx1/pull/3) · [Nintendo Safe Mode #55](https://github.com/Garyu-tt/EXODUSx1/issues/55)

The Steam File Check homebrew application compares the listed game files on SD with a retained reference. Its own README covers reference generation and on-device verification. [Steam File Check](../../steam_integrity/README.md)

## Launch from an inserted game card

Nintendo Safe Mode admits one Switch catalog launch provider. With no application suspended, insert a game card and select **Launch** on the page containing a ready title from that card. Choose its number and confirm the name and hexadecimal ID read from local control data. uMenu asks the matching uSystem to discover and queue the catalog route. uSystem rechecks the provider and selected user when it consumes the queued action, immediately before starting the title.

The gate requires a canonical Switch application ID, a matching registered Horizon application and view, exact membership on the currently inserted card, a ready launch version and complete control data from local storage. It denies incomplete evidence, startup network licensing or network account requirements, required repairs or updates, and suspended applications. The catalog route uses local provider queries and has its own uSystem command. Digital titles, Steam and unclassified catalog rows remain available for browsing. The provider review is limited to this EXODUS entry point. [EX-006 tracker](https://github.com/Garyu-tt/EXODUSx1/pull/3) · [Nintendo Safe Mode #55](https://github.com/Garyu-tt/EXODUSx1/issues/55) · [Pinned libnx NS API](https://github.com/switchbrew/libnx/blob/b156b4774fb2df7b93402169509266da7547fc4c/nx/include/switch/services/ns.h)

## Launch local homebrew as an applet

Add a local NRO under `/switch/` to the root of the selected user's uLaunch grid. The catalog's **Launch** list includes it when a `switch_homebrew` row matches exactly one live registration and the NRO is present and passes the loader format checks. Confirm the registered path displayed by uMenu. Use the default uLaunch library-applet configuration.

uSystem checks the same local registration and file before queueing and again before handing off to the matching uLoader applet. The backend constructs the single path argument from the registered ID. The patched loader accepts a dedicated one-shot input for the default Album library applet and checks the image, BSS and page padding against the allocated applet heap before reading the payload. It returns to HOME after the target exits. [Nintendo Safe Mode #55](https://github.com/Garyu-tt/EXODUSx1/issues/55)

## Build the matching uMenu, uSystem and uLoader applet

The source patch targets uLaunch 1.2.6 and uses its pinned Plutonium, libnx-ext and nlohmann/json submodules. The build uses the upstream devkitPro and Python dependencies and needs the source libnx layout for NACP language data. [uLaunch release](https://github.com/XorTroll/uLaunch/releases/tag/1.2.6) · [Upstream build workflow](https://github.com/XorTroll/uLaunch/blob/6e0e1de633eec601fb527aadb602abe3bc817541/.github/workflows/main.yml) · [Tested libnx revision](https://github.com/switchbrew/libnx/tree/b156b4774fb2df7b93402169509266da7547fc4c)

```sh
adapter="$PWD/exodus/home/ulaunch"
git clone --branch 1.2.6 --depth 1 https://github.com/XorTroll/uLaunch.git
test "$(git -C uLaunch rev-parse HEAD)" = 6e0e1de633eec601fb527aadb602abe3bc817541 || exit 1
git -C uLaunch submodule update --init --recursive --depth 1 arc libs/Plutonium libs/libnx-ext libs/json libs/zip libs/Atmosphere-libs
git -C uLaunch apply --unidiff-zero --check "$adapter/quick-menu.patch"
git -C uLaunch apply --unidiff-zero "$adapter/quick-menu.patch"
cp "$adapter"/catalog.hpp "$adapter"/catalog.cpp "$adapter"/catalog_view.cpp \
  "$adapter"/catalog_refresh.hpp "$adapter"/catalog_refresh.cpp \
  uLaunch/projects/uMenu/source/ul/menu/ui/
cp "$adapter"/catalog_homebrew_policy.hpp uLaunch/libs/uCommon/include/ul/menu/
cp "$adapter"/catalog_steam_install.hpp uLaunch/libs/uCommon/include/ul/menu/
cp "$adapter"/catalog_gamecard_policy.hpp "$adapter"/catalog_homebrew_policy.hpp \
  "$adapter"/catalog_homebrew_provider.hpp "$adapter"/loader_nro_bounds.hpp uLaunch/projects/uSystem/source/
cp "$adapter"/catalog_steam_install.hpp "$adapter"/catalog_steam_install.cpp uLaunch/projects/uSystem/source/
cp "$adapter"/loader_nro_bounds.hpp uLaunch/projects/uLoader/source/ul/loader/

git init libnx
git -C libnx fetch --depth 1 https://github.com/switchbrew/libnx.git b156b4774fb2df7b93402169509266da7547fc4c
git -C libnx checkout --detach FETCH_HEAD
make -C libnx
cp -r libnx/nx/external/bsd/include/. libnx/nx/include/
PYTHONUTF8=1 make -C uLaunch LIBNX="$PWD/libnx/nx" umenu usystem uloader
```

Use a test SD with uLaunch 1.2.6. Save the originals and install `SdOut/ulaunch/bin/uMenu/`, both uSystem files, and `SdOut/ulaunch/bin/uLoader/applet/` together. The catalog homebrew route requires this matching applet loader. The patch and adapter use GPL-2.0-only. [COPYING](COPYING)

## Check the adapter on a host

```sh
make -C "$adapter" check CPPFLAGS="-I$PWD/uLaunch/libs/json/single_include/nlohmann"
make -C "$adapter" check-launch-policy CXXFLAGS="-O2 -Werror -fsanitize=undefined"
```

On a Switch running the matching uLaunch release, start with a manual catalog containing Steam and Switch or homebrew entries. Open **EXODUS library** to check the baseline, sync from the NRO, then reopen the library to confirm the Steam rows update and the local entries remain. Repeat with an authorized Steam account whose shared library is empty to check Steam row removal. On a test SD card, add an invalid file with a newer snapshot number and verify that the last valid library still opens.

Check that **Launch** appears for a ready title on an inserted card and opens it after confirmation. Check cancellation, a stale ID, an ejected card, an installed digital title and a suspended application. Check that the normal HOME grid still opens a title. For rollback, power off and restore the saved uMenu and uLoader applet directories and both uSystem files.

For homebrew, use an NRO registered in the root grid with spaces in its filename. Check the displayed path, cancellation and a confirmed applet launch, then remove the grid registration while keeping the catalog row and check that it disappears from the launch list. Try the same check with a missing NRO and verify that the normal grid still handles its own entries.

For Steam, open **Steam status** on a page with both Steam and local entries, select a specific game and check its displayed AppID and saved-library source. Repeat with the manual catalog and check for that source. With a Steam-created manifest for that AppID and its matching game directory on the test SD, verify the matching evidence. Rename the game directory and check the missing-directory result. Check another Steam entry with no local manifest. In each case launch stays blocked. The launch list should contain eligible local entries only.

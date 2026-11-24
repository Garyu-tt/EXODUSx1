# EXODUS library in uLaunch HOME

The adapter adds **EXODUS library** to the Quick Menu in uLaunch's `uMenu` HOME interface. From the main uMenu screen, open Quick Menu with **ZL** or **ZR** and select **EXODUS library**. The dialog displays application names and providers in pages of six. **Previous**, **Next** and **Close** use uMenu's controller dialog.

The viewer opens `/ulaunch/exodus-catalog.json` on the SD card when selected. Use a version 1 snapshot from the EXODUS Steam importer, or combine it with system and homebrew inventory snapshots using the EXODUS catalog merger. The viewer accepts up to 2 MiB and 4,096 entries. It requires the eight version 1 entry fields, preserves order, rejects duplicate identities and opens an error dialog for unreadable or malformed snapshots. Display names are shortened to fit. The catalog carries snapshot metadata on the console. Live ownership, downloads and launch routing are separate work. [Catalog merger and Switch exporter](https://github.com/Garyu-tt/EXODUSx1/pull/15) · [Homebrew inventory](https://github.com/Garyu-tt/EXODUSx1/pull/13) · [EX-006](https://github.com/Garyu-tt/EXODUSx1/pull/3)

The viewer accepts UTF-8 catalogs and BOM-marked UTF-16LE catalogs, including snapshots saved by redirecting importer output in Windows PowerShell 5.1.

## Automatic Steam refresh

Use the patched `uMenu` together with the current EXODUS Steam Sync NRO. Each successful sync publishes a Steam-only snapshot in `/switch/exodus-steam-sync/snapshots/` after validation and flushing the staged file. Reopen **EXODUS library** to read the latest usable Steam library and the current `/ulaunch/exodus-catalog.json`. Synced Steam entries appear first, followed by the existing non-Steam entries in their original order. A synced empty Steam library removes the Steam entries from the view. [Steam Sync build and sign-in](../../steam_switch/README.md)

The viewer skips incomplete, malformed and oversized Steam snapshots, as well as snapshots that would exceed the 4,096-entry combined limit. With no usable Steam snapshot it displays the manual catalog. With no manual catalog it can display the Steam snapshot by itself. An existing invalid manual catalog still shows its catalog error. The NRO publishes into its own directory and keeps the latest eight valid generations when cleanup succeeds.

To restore the manual catalog view, close the NRO and move the `snapshots` directory to a backup location. Both apps leave `/ulaunch/exodus-catalog.json` and earlier manually exported catalogs intact. A later successful sync creates a new snapshot directory.

## Launch installed Switch titles

Choose **Launch** on a page with registered Switch titles, then choose its number. The confirmation dialog displays the title from uLaunch's local application metadata and its hexadecimal application ID. After confirmation, uMenu checks the live Horizon application record and view again and reuses the HOME grid's launch checks for content, game cards, updates and suspended applications.

Launch candidates come from catalog rows with provider `switch` and a nonzero 16-digit hexadecimal `provider_id` present in Horizon's application registry. The catalog page also displays Steam, homebrew and other providers. Their rows have no launch action in this adapter. [EX-006 tracker](https://github.com/Garyu-tt/EXODUSx1/pull/3) · [Nintendo Safe Mode provider review](https://github.com/Garyu-tt/EXODUSx1/issues/55)

## Build the patched uMenu

The source patch targets uLaunch 1.2.6 and uses its pinned Plutonium, libnx-ext and nlohmann/json submodules. The build uses the upstream devkitPro and Python dependencies and needs the source libnx layout for NACP language data. [uLaunch release](https://github.com/XorTroll/uLaunch/releases/tag/1.2.6) · [Upstream build workflow](https://github.com/XorTroll/uLaunch/blob/6e0e1de633eec601fb527aadb602abe3bc817541/.github/workflows/main.yml) · [Tested libnx revision](https://github.com/switchbrew/libnx/tree/b156b4774fb2df7b93402169509266da7547fc4c)

```sh
adapter="$PWD/exodus/home/ulaunch"
git clone --branch 1.2.6 --depth 1 https://github.com/XorTroll/uLaunch.git
test "$(git -C uLaunch rev-parse HEAD)" = 6e0e1de633eec601fb527aadb602abe3bc817541 || exit 1
git -C uLaunch submodule update --init --depth 1 arc libs/Plutonium libs/libnx-ext libs/json libs/zip
git -C uLaunch apply --unidiff-zero --check "$adapter/quick-menu.patch"
git -C uLaunch apply --unidiff-zero "$adapter/quick-menu.patch"
cp "$adapter"/catalog.hpp "$adapter"/catalog.cpp "$adapter"/catalog_view.cpp \
  "$adapter"/catalog_refresh.hpp "$adapter"/catalog_refresh.cpp \
  uLaunch/projects/uMenu/source/ul/menu/ui/

git init libnx
git -C libnx fetch --depth 1 https://github.com/switchbrew/libnx.git b156b4774fb2df7b93402169509266da7547fc4c
git -C libnx checkout --detach FETCH_HEAD
make -C libnx
cp -r libnx/nx/external/bsd/include/. libnx/nx/include/
PYTHONUTF8=1 make -C uLaunch LIBNX="$PWD/libnx/nx" umenu
```

The target produces `uLaunch/SdOut/ulaunch/bin/uMenu/` for the matching uLaunch release. Keep the original uMenu directory to restore the release bundle. The patch and adapter use GPL-2.0-only. [COPYING](COPYING)

## Check the adapter on a host

```sh
make -C "$adapter" check CPPFLAGS="-I$PWD/uLaunch/libs/json/single_include/nlohmann"
```

On a Switch running the matching uLaunch release, start with a manual catalog containing Steam and Switch or homebrew entries. Open **EXODUS library** to check the baseline, sync from the NRO, then reopen the library to confirm the Steam rows update and the local entries remain. Repeat with an authorized Steam account whose shared library is empty to check Steam row removal. On a test SD card, add an invalid file with a newer snapshot number and verify that the last valid library still opens.

Check that **Launch** can select a registered title from the catalog, that cancelling the confirmation returns to the same page, and that the confirmed title opens. Exercise a stale title ID and an ejected game card. Check that the normal HOME grid still opens a title, then restore the backed-up uMenu directory.

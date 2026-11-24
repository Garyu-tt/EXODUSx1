# EXODUS library in uLaunch HOME

The adapter adds **EXODUS library** to the Quick Menu in uLaunch's `uMenu` HOME interface. From the main uMenu screen, open Quick Menu with **ZL** or **ZR** and select **EXODUS library**. The dialog displays application names and providers in pages of six. **Previous**, **Next** and **Close** use uMenu's controller dialog. Native titles use uMenu's existing HOME grid launch path.

The viewer opens `/ulaunch/exodus-catalog.json` on the SD card when selected. Use a version 1 snapshot from the EXODUS Steam importer, or combine it with system and homebrew inventory snapshots using the EXODUS catalog merger. The viewer accepts up to 2 MiB and 4,096 entries. It requires the eight version 1 entry fields, preserves order, rejects duplicate identities and opens an error dialog for unreadable or malformed snapshots. Display names are shortened to fit. The catalog carries snapshot metadata on the console. Live ownership, downloads and launch routing are separate work. [Catalog merger and Switch exporter](https://github.com/Garyu-tt/EXODUSx1/pull/15) · [Homebrew inventory](https://github.com/Garyu-tt/EXODUSx1/pull/13) · [EX-006](https://github.com/Garyu-tt/EXODUSx1/pull/3)

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
  uLaunch/projects/uMenu/source/ul/menu/ui/

git init libnx
git -C libnx fetch --depth 1 https://github.com/switchbrew/libnx.git b156b4774fb2df7b93402169509266da7547fc4c
git -C libnx checkout --detach FETCH_HEAD
make -C libnx
cp -r libnx/nx/external/bsd/include/. libnx/nx/include/
PYTHONUTF8=1 make -C uLaunch LIBNX="$PWD/libnx/nx" umenu
```

The target produces `uLaunch/SdOut/ulaunch/bin/uMenu/` for the matching uLaunch release. Keep the original uMenu directory to restore the release bundle. Device acceptance should confirm the Quick Menu item, empty and malformed snapshot behavior, page navigation, native title launch and restoration of the original bundle. The patch and adapter use GPL-2.0-only. [COPYING](COPYING)

## Check the parser on a host

```sh
make -C "$adapter" check CPPFLAGS="-I$PWD/uLaunch/libs/json/single_include/nlohmann"
```

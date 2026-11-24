#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Standalone devkitA64 build. Produces an ordinary NRO, never a HOME replacement.
set -eu
if [ "$#" -ne 2 ]; then
    echo "usage: sh $0 CORKDECK_SOURCE OUTPUT_DIRECTORY" >&2
    exit 2
fi
: "${DEVKITPRO:?Set DEVKITPRO to the devkitPro root}"
output="$2"
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
project=$(CDPATH= cd -- "$here/../../.." && pwd -P)
donor=$(CDPATH= cd -- "$1" && pwd -P)
export LC_ALL=C TZ=UTC
export SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-0}"
sdk="${DEVKITA64:-$DEVKITPRO/devkitA64}"
nx="$DEVKITPRO/libnx"
port="$DEVKITPRO/portlibs/switch"
cc="$sdk/bin/aarch64-none-elf-gcc"
cxx="$sdk/bin/aarch64-none-elf-g++"
nacp="$DEVKITPRO/tools/bin/nacptool"
nro="$DEVKITPRO/tools/bin/elf2nro"
for tool in "$cc" "$cxx" "$nacp" "$nro"; do
    [ -x "$tool" ] || { echo "Missing SDK tool: $tool" >&2; exit 1; }
done
[ -f "$nx/switch.specs" ] || { echo "Missing libnx switch.specs" >&2; exit 1; }
json_include="${JSON_INCLUDE:-/usr/include/nlohmann}"
[ -f "$json_include/json.hpp" ] || { echo "Set JSON_INCLUDE to the directory containing json.hpp" >&2; exit 1; }
# Never resolve host libraries when cross-compiling.
export PKG_CONFIG_PATH=
export PKG_CONFIG_LIBDIR="$port/lib/pkgconfig:$port/share/pkgconfig"
unset PKG_CONFIG_SYSROOT_DIR
packages="sdl2 SDL2_ttf libpng libturbojpeg"
pkg-config --exists $packages || { echo "Missing Switch portlibs: $packages" >&2; exit 1; }
cflags=$(pkg-config --cflags $packages)
libs=$(pkg-config --static --libs $packages)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
trap 'exit 1' HUP INT TERM
tmp=$(CDPATH= cd -- "$tmp" && pwd -P)
"${PYTHON:-python3}" "$here/prepare-donor.py" "$donor" "$tmp/corkdeck-donor"
src="$tmp/corkdeck-donor/horizon-wine/source"
# nlohmann uses <nlohmann/...> internally. Stage only its header tree;
# adding the host /usr/include would contaminate the ARM64 sysroot.
mkdir -p "$tmp/json-headers/nlohmann"
cp -R "$json_include/." "$tmp/json-headers/nlohmann/"

# Normalize paths in diagnostics and debug metadata, including staged JSON headers.
# Reject date/time macros so a source file's mtime cannot enter the artifact.
set -- "-ffile-prefix-map=$(pwd -P)=/usr/src/exodus-build/working-directory" \
    "-ffile-prefix-map=$project=/usr/src/EXODUSx1" \
    "-ffile-prefix-map=$tmp=/usr/src/exodus-build" -Werror=date-time
arch="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE"
for file in "$here/settings_preview.c" "$here/windows_pe.c" "$here/../../runtime/autorun_probe.c" \
    "$here/focus_adapter.c" "$here/focus_indicator.c" \
    "$src/launcher_ui.c" "$src/launcher_audio.c" "$src/launcher_svg.c" "$src/launcher_image.c"; do
    object="$(basename "$file").o"
    "$cc" $arch -D__SWITCH__ -D_REENTRANT -std=gnu11 -O2 -Wall -Wextra "$@" "-frandom-seed=$object" \
        -ffunction-sections -fdata-sections -I"$nx/include" -I"$src" $cflags \
        -c "$file" -o "$tmp/$object"
done
for file in "$here/library_catalog.cpp" "$here/../ulaunch/catalog.cpp"; do
    object="$(basename "$file").o"
    "$cxx" $arch -D__SWITCH__ -std=c++17 -O2 -Wall -Wextra "$@" "-frandom-seed=$object" \
        -ffunction-sections -fdata-sections -I"$tmp/json-headers" -I"$tmp/json-headers/nlohmann" -I"$nx/include" \
        -c "$file" -o "$tmp/$object"
done
"$cxx" $arch -specs="$nx/switch.specs" -Wl,--gc-sections "$tmp"/*.o \
    -L"$port/lib" -L"$nx/lib" -Wl,--start-group $libs ${SWITCH_EXTRA_LIBS:-} -lnx -lm -Wl,--end-group \
    -Wl,--build-id=sha1 -o "$tmp/exodus-shell.elf"
"$nacp" --create "EXODUS Shell" "EXODUSx1" "0.10.0" "$tmp/exodus-shell.nacp"
"$nro" "$tmp/exodus-shell.elf" "$tmp/exodus-shell.nro" \
    --nacp="$tmp/exodus-shell.nacp"
mkdir -p "$output"
cp "$tmp/exodus-shell.elf" "$tmp/exodus-shell.nro" "$output/"
printf '%s\n' "Built $output/exodus-shell.nro; device acceptance required."

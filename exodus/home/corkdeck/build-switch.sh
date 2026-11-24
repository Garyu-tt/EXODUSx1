#!/bin/sh
# Standalone devkitA64 build. Produces an ordinary NRO, never a HOME replacement.
set -eu
if [ "$#" -ne 2 ]; then
    echo "usage: sh $0 CORKDECK_CHECKOUT OUTPUT_DIRECTORY" >&2
    exit 2
fi
: "${DEVKITPRO:?Set DEVKITPRO to the devkitPro root}"
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
donor=$(CDPATH= cd -- "$1" && pwd)
pin=adc0b52284e9e57d18808427a770a2a162c76bf0
[ "$(git -C "$donor" rev-parse HEAD)" = "$pin" ] || {
    echo "Wrong CorkDeck revision; expected $pin" >&2
    exit 1
}
git -C "$donor" diff --quiet HEAD -- horizon-wine/source || {
    echo "Donor source has local changes" >&2
    exit 1
}
sdk="${DEVKITA64:-$DEVKITPRO/devkitA64}"
nx="$DEVKITPRO/libnx"
port="$DEVKITPRO/portlibs/switch"
src="$donor/horizon-wine/source"
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
# nlohmann uses <nlohmann/...> internally. Stage only its header tree;
# adding the host /usr/include would contaminate the ARM64 sysroot.
mkdir -p "$tmp/json-headers/nlohmann"
cp -R "$json_include/." "$tmp/json-headers/nlohmann/"
arch="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE"
for file in "$here/settings_preview.c" "$here/../../runtime/autorun_probe.c" \
    "$src/launcher_ui.c" "$src/launcher_audio.c" "$src/launcher_svg.c" "$src/launcher_image.c"; do
    "$cc" $arch -D__SWITCH__ -D_REENTRANT -std=gnu11 -O2 -Wall -Wextra \
        -ffunction-sections -fdata-sections -I"$nx/include" -I"$src" $cflags \
        -c "$file" -o "$tmp/$(basename "$file").o"
done
for file in "$here/library_catalog.cpp" "$here/../ulaunch/catalog.cpp"; do
    "$cxx" $arch -D__SWITCH__ -std=c++17 -O2 -Wall -Wextra \
        -ffunction-sections -fdata-sections -I"$tmp/json-headers" -I"$tmp/json-headers/nlohmann" -I"$nx/include" \
        -c "$file" -o "$tmp/$(basename "$file").o"
done
"$cxx" $arch -specs="$nx/switch.specs" -Wl,--gc-sections "$tmp"/*.o \
    -L"$port/lib" -L"$nx/lib" -Wl,--start-group $libs ${SWITCH_EXTRA_LIBS:-} -lnx -lm -Wl,--end-group \
    -o "$tmp/exodus-shell.elf"
"$nacp" --create "EXODUS Shell" "EXODUSx1" "0.17.0" "$tmp/exodus-shell.nacp"
"$nro" "$tmp/exodus-shell.elf" "$tmp/exodus-shell.nro" \
    --icon="$nx/default_icon.jpg" --nacp="$tmp/exodus-shell.nacp"
mkdir -p "$2"
cp "$tmp/exodus-shell.elf" "$tmp/exodus-shell.nro" "$2/"
printf '%s\n' "Built $2/exodus-shell.nro; device acceptance required."

#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
# Build the EXODUS Shell for the host with the prepared CorkDeck UI.
set -eu
if [ "$#" -ne 2 ]; then
    echo "usage: sh $0 CORKDECK_SOURCE OUTPUT_BINARY" >&2
    exit 2
fi
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
donor=$(CDPATH= cd -- "$1" && pwd)
# Compile donor C as C, catalog parser as C++17, then link together.
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
trap 'exit 1' HUP INT TERM
python3 "$here/prepare-donor.py" "$donor" "$tmp/corkdeck-donor"
src="$tmp/corkdeck-donor/horizon-wine/source"
json_include="${JSON_INCLUDE:-/usr/include/nlohmann}"
# shellcheck disable=SC2046
for file in "$here/settings_preview.c" "$here/focus_adapter.c" "$here/focus_indicator.c" \
    "$here/../../runtime/autorun_probe.c" "$src/launcher_ui.c" "$src/launcher_audio.c" \
    "$src/launcher_svg.c" "$src/launcher_image.c"; do
    "${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -I "$src" \
        $(pkg-config --cflags sdl2 SDL2_ttf libpng libturbojpeg) -c "$file" -o "$tmp/$(basename "$file").o"
done
for file in "$here/library_catalog.cpp" "$here/../ulaunch/catalog.cpp"; do
    "${CXX:-c++}" -std=c++17 -O2 -Wall -Wextra -I "$json_include" -I "$(dirname -- "$json_include")" \
        -c "$file" -o "$tmp/$(basename "$file").o"
done
# shellcheck disable=SC2046
"${CXX:-c++}" "$tmp"/*.o $(pkg-config --libs sdl2 SDL2_ttf libpng libturbojpeg) -lm -o "$2"

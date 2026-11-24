#!/bin/sh
# Build the original CorkDeck UI primitives without Wine or its launcher.
set -eu
if [ "$#" -ne 2 ]; then
    echo "usage: sh $0 CORKDECK_CHECKOUT OUTPUT_BINARY" >&2
    exit 2
fi
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
donor=$(CDPATH= cd -- "$1" && pwd)
pin=adc0b52284e9e57d18808427a770a2a162c76bf0
[ "$(git -C "$donor" rev-parse HEAD)" = "$pin" ] || {
    echo "CorkDeck checkout must be pinned to $pin" >&2
    exit 1
}
git -C "$donor" diff --quiet HEAD -- horizon-wine/source || {
    echo "Donor source has local changes" >&2
    exit 1
}
src="$donor/horizon-wine/source"
# pkg-config supplies host SDL2 and SDL2_ttf; no runtime or Wine sources.
# shellcheck disable=SC2046
"${CC:-cc}" -std=gnu11 -O2 -Wall -Wextra -I "$src" \
    $(pkg-config --cflags sdl2 SDL2_ttf) \
    "$here/settings_preview.c" "$src/launcher_ui.c" \
    "$src/launcher_audio.c" "$src/launcher_svg.c" \
    $(pkg-config --libs sdl2 SDL2_ttf) -lm -o "$2"

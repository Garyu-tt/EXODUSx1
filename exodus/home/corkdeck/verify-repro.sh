#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-only
set -eu
if [ "$#" -ne 3 ]; then
    printf '%s\n' "usage: sh $0 EXODUS_SOURCE_DIRECTORY CORKDECK_SOURCE OUTPUT_DIRECTORY" >&2
    exit 2
fi

source_dir=$(CDPATH= cd -- "$1" && pwd -P)
donor=$(CDPATH= cd -- "$2" && pwd -P)
output="$3"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
trap 'exit 1' HUP INT TERM
tmp=$(CDPATH= cd -- "$tmp" && pwd -P)

build_copy() (
    build_root="$1"
    project="$build_root/source"
    mkdir -p "$project/exodus/home/ulaunch" "$project/exodus/runtime" "$build_root/tmp"
    cp -R "$source_dir/exodus/home/corkdeck" "$project/exodus/home/"
    cp "$source_dir/exodus/home/ulaunch/catalog.cpp" "$source_dir/exodus/home/ulaunch/catalog.hpp" \
        "$source_dir/exodus/home/ulaunch/catalog_refresh.cpp" "$source_dir/exodus/home/ulaunch/catalog_refresh.hpp" \
        "$source_dir/exodus/home/ulaunch/COPYING" \
        "$project/exodus/home/ulaunch/"
    cp "$source_dir/exodus/runtime/autorun_probe.c" "$source_dir/exodus/runtime/autorun_probe.h" \
        "$project/exodus/runtime/"
    cd -- "$project"
    TMPDIR="$build_root/tmp" sh exodus/home/corkdeck/build-switch.sh "$donor" "$build_root/output"
)

first="$tmp/first"
second="$tmp/second build"
build_copy "$first"
build_copy "$second"

for artifact in exodus-shell.nro exodus-shell.elf; do
    if ! cmp "$first/output/$artifact" "$second/output/$artifact"; then
        printf '%s\n' "$artifact differs across build paths" >&2
        exit 1
    fi
done

mkdir -p "$output"
cp "$first/output/exodus-shell.nro" "$first/output/exodus-shell.elf" "$output/"
printf '%s\n' "NRO and ELF matched across both build paths. Verified artifacts in $output."

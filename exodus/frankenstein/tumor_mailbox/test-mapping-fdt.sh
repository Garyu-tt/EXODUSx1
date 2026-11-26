#!/bin/sh
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
for role in 1 2; do
    sh "$here/prepare-mapping-fdt.sh" "$role" "$work/role-$role.dtb"
done
test "$(fdtget -t x "$work/role-1.dtb" /memory@60000000 reg)" = "0 60000000 0 10000000"
test "$(fdtget -t x "$work/role-2.dtb" /memory@60000000 reg)" = "0 60000000 0 20000000"
for bad in 0 3 other; do
    if sh "$here/prepare-mapping-fdt.sh" "$bad" "$work/invalid.dtb"; then exit 1; fi
    test ! -e "$work/invalid.dtb"
done
echo EXODUS_MAILBOX_MAPPING_FDT_CONTROL_PASS

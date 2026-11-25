#!/bin/sh
# Build ONLY the two fixed Bao/QEMU reference FDTs for mapping-only control.
# The resulting node is deliberately not a coherent-memory assertion.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
test "$#" -ge 2
test "$#" -le 3
mode=${3:-mapping}
case "$mode" in
    mapping) flag=exodus,mapping-only ;;
    exchange) test "$1" = 2; flag=exodus,nc-exchange-control ;;
    *) exit 2 ;;
esac
case "$1" in
    1) source="$here/../bao_freebsd_headless.dts" ;;
    2) source="$here/../bao_freebsd_guest.dts" ;;
    *) echo "expected role 1 or 2" >&2; exit 2 ;;
esac
role=$1
out=$2
tmp=$(mktemp "$out.XXXXXX")
trap 'rm -f "$tmp"' EXIT
dtc -I dts -O dtb -o "$tmp" "$source"
test "$(fdtget -t s "$tmp" / model)" = "Tumor Bao QEMU reference"
node=/mailbox@f0000000
if fdtget -l "$tmp" / | grep -Fx mailbox@f0000000; then exit 1; fi
fdtput -c "$tmp" "$node"
fdtput -t s "$tmp" "$node" compatible exodus,tumor-mailbox-reference-v1
fdtput -t x "$tmp" "$node" reg 0 f0000000 0 10000
fdtput -t x "$tmp" "$node" exodus,role "$role"
fdtput -t s "$tmp" "$node" status okay
fdtput "$tmp" "$node" "$flag"
test "$(fdtget -t x "$tmp" "$node" reg)" = "0 f0000000 0 10000"
test "$(fdtget -t x "$tmp" "$node" exodus,role)" = "$role"
test "$(fdtget -t s "$tmp" "$node" status)" = okay
fdtget "$tmp" "$node" "$flag" >/dev/null
if [ "$mode" = exchange ]; then
    if fdtget "$tmp" "$node" exodus,mapping-only >/dev/null 2>&1; then exit 1; fi
fi
if fdtget "$tmp" "$node" exodus,coherent-normal-memory >/dev/null 2>&1; then exit 1; fi
mv "$tmp" "$out"
echo "EXODUS_MAILBOX_MAPPING_FDT_PASS role=$role"

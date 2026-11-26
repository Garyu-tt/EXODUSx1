#!/bin/sh
set -eu
trap 'rc=$?; echo "EXODUS_ARMV7_KERNEL_GATE_RESULT=$rc"' EXIT
test "$(uname -s)" = FreeBSD
test "$(uname -p)" = armv7
test "$(freebsd-version -k)" = 15.1-RELEASE
uname -a
mkdir -p /var/tmp/exodus-kernel-gate /mnt/exodus-seed
cd /var/tmp/exodus-kernel-gate
mount -t msdosfs -o ro /dev/msdosfs/EXODUSSEED /mnt/exodus-seed
cp /mnt/exodus-seed/exodus_amp1.c /mnt/exodus-seed/amp_execution_contract.c /mnt/exodus-seed/Makefile /mnt/exodus-seed/test_kernel_amp.c .
test "$(sha256 -q /mnt/exodus-seed/src.txz)" = cf5762da53fd52e1eaf0f9ceee9bf58cbe314c821031d0d9ffa76823185a89e1
mkdir source
tar -xJf /mnt/exodus-seed/src.txz -C source
umount /mnt/exodus-seed
make SYSDIR="$PWD/source/usr/src/sys" all
test -s exodus_amp1.ko
readelf -h exodus_amp1.ko
readelf -h exodus_amp1.ko | grep -E 'Class:[[:space:]]+ELF32'
readelf -h exodus_amp1.ko | grep -E 'Machine:[[:space:]]+ARM'
cc -std=c11 -O2 -Wall -Wextra test_kernel_amp.c -o test_kernel_amp
kldload "$PWD/exodus_amp1.ko"
kldstat -n exodus_amp1.ko
./test_kernel_amp
kldunload exodus_amp1.ko
if kldstat -n exodus_amp1.ko >/dev/null 2>&1; then exit 1; fi
if sysctl -n kern.exodus_amp1_count >/dev/null 2>&1; then exit 1; fi
echo EXODUS_ARMV7_KERNEL_UNLOAD_PASS

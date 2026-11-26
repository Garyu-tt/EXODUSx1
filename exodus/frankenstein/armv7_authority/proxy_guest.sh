#!/bin/sh
set -eu
trap 'rc=$?; echo "EXODUS_PROXY_RESULT=$rc"' EXIT
cd /var/tmp/exodus-kernel-gate
mkdir proxy
mount -t msdosfs -o ro /dev/msdosfs/EXODUSSEED /mnt/exodus-seed
cp /mnt/exodus-seed/exodus_proxy.c /mnt/exodus-seed/test_proxy.c proxy/
cp /mnt/exodus-seed/Makefile.proxy proxy/Makefile
umount /mnt/exodus-seed
cd proxy
make SYSDIR="/var/tmp/exodus-kernel-gate/source/usr/src/sys" all
cc -std=c11 -O2 -Wall -Wextra -Werror test_proxy.c -o test_proxy
kldload "$PWD/exodus_proxy.ko"
timeout 60 ./test_proxy
kldunload exodus_proxy.ko
if sysctl -n kern.exodus_proxy_snapshot >/dev/null 2>&1; then exit 1; fi
echo EXODUS_PROXY_UNLOAD_PASS

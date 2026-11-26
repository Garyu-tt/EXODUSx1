#!/bin/sh
set -eu
trap 'rc=$?; echo "EXODUS_AWT_RESULT=$rc"' EXIT
cd /var/tmp/exodus-kernel-gate
mkdir awt
mount -t msdosfs -o ro /dev/msdosfs/EXODUSSEED /mnt/exodus-seed
cp /mnt/exodus-seed/exodus_awt.c /mnt/exodus-seed/test_awt.c /mnt/exodus-seed/amp_wait_contract.h awt/
cp /mnt/exodus-seed/Makefile.awt awt/Makefile
umount /mnt/exodus-seed
cd awt
make SYSDIR="/var/tmp/exodus-kernel-gate/source/usr/src/sys" all
cc -std=c11 -O2 -Wall -Wextra -Werror test_awt.c -o test_awt
kldload "$PWD/exodus_awt.ko"
timeout 60 ./test_awt
kldunload exodus_awt.ko
if sysctl -n kern.exodus_awt_exchange >/dev/null 2>&1; then exit 1; fi
echo EXODUS_AWT_UNLOAD_PASS

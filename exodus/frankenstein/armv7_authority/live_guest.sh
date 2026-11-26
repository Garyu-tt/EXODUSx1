#!/bin/sh
set -eu
trap 'rc=$?; stty echo; echo "EXODUS_LIVE_RESULT=$rc"' EXIT
cd /var/tmp/exodus-kernel-gate
mount -t msdosfs -o ro /dev/msdosfs/EXODUSSEED /mnt/exodus-seed
cp /mnt/exodus-seed/live_relay.c .
umount /mnt/exodus-seed
cc -std=c11 -O2 -Wall -Wextra -Werror live_relay.c -o live_relay
kldload "$PWD/exodus_amp1.ko"
stty -echo
./live_relay
kldunload exodus_amp1.ko
if sysctl -n kern.exodus_amp1_count >/dev/null 2>&1; then exit 1; fi
echo EXODUS_LIVE_UNLOAD_PASS

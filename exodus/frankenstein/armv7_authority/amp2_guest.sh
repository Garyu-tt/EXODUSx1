#!/bin/sh
set -eu
trap 'rc=$?; stty echo; echo "EXODUS_LIVE_RESULT=$rc"' EXIT
cd /var/tmp/exodus-kernel-gate
mount -t msdosfs -o ro /dev/msdosfs/EXODUSSEED /mnt/exodus-seed
mkdir -p amp2
cp /mnt/exodus-seed/amp2_relay.c /mnt/exodus-seed/exodus_amp2.c /mnt/exodus-seed/amp2_pid_contract.c amp2/
cp /mnt/exodus-seed/Makefile.amp2 amp2/Makefile
umount /mnt/exodus-seed
cd amp2
make SYSDIR="/var/tmp/exodus-kernel-gate/source/usr/src/sys" all
cc -std=c11 -O2 -Wall -Wextra -Werror amp2_relay.c -o live_relay
kldload "$PWD/exodus_amp2.ko"
stty -echo
./live_relay
kldunload exodus_amp2.ko
if sysctl -n kern.exodus_amp2_count >/dev/null 2>&1; then exit 1; fi
echo EXODUS_LIVE_UNLOAD_PASS

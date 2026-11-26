#!/bin/sh
set -eu
trap 'rc=$?; stty echo; echo "EXODUS_LIVE_RESULT=$rc"' EXIT
cd /var/tmp/exodus-kernel-gate
mkdir awt
mount -t msdosfs -o ro /dev/msdosfs/EXODUSSEED /mnt/exodus-seed
cp /mnt/exodus-seed/exodus_awt_poll.c /mnt/exodus-seed/awt_poll_relay.c /mnt/exodus-seed/amp_wait_contract.h awt/
cp /mnt/exodus-seed/Makefile.awtpoll awt/Makefile
umount /mnt/exodus-seed
cd awt
make SYSDIR="/var/tmp/exodus-kernel-gate/source/usr/src/sys" all
cc -std=c11 -O2 -Wall -Wextra -Werror awt_poll_relay.c -o awt_poll_relay
kldload "$PWD/exodus_awt_poll.ko"
stty -echo
./awt_poll_relay
kldunload exodus_awt_poll.ko
if sysctl -n kern.exodus_awt_exchange >/dev/null 2>&1; then exit 1; fi
echo EXODUS_LIVE_UNLOAD_PASS

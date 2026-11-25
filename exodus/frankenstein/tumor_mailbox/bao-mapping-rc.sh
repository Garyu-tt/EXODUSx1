#!/bin/sh
# QEMU Bao RAM-root control only. No shared-memory reads or writes.
set -eu
export PATH=/rescue:/bin:/sbin
test "$(/rescue/sysctl -n kern.ostype)" = FreeBSD
test -c /dev/console
if /rescue/kldstat -n tumor_mailbox.ko >/dev/null 2>&1; then exit 1; fi
if /rescue/sysctl -n 'dev.tumor_mailbox.0.%driver' >/dev/null 2>&1; then exit 1; fi
/rescue/kldload /boot/kernel/tumor_mailbox.ko
/rescue/kldstat -m ofwbus/tumor_mailbox
test "$(/rescue/sysctl -n 'dev.tumor_mailbox.0.%driver')" = tumor_mailbox
printf '%s\n' 'TUMOR_BAO_REF: MAPPING_ONLY_DRIVER_ATTACHED' > /dev/console
/rescue/kldunload tumor_mailbox.ko
if /rescue/kldstat -m ofwbus/tumor_mailbox >/dev/null 2>&1; then exit 1; fi
if /rescue/sysctl -n 'dev.tumor_mailbox.0.%driver' >/dev/null 2>&1; then exit 1; fi
printf '%s\n' 'TUMOR_BAO_REF: MAPPING_ONLY_DRIVER_DETACHED' > /dev/console
exit 0

#!/bin/sh
# Minimal first-boot gate: no package downloads, no application compilation.
set -eu
exec >/dev/console 2>&1
finish() {
    rc=$?
    trap - EXIT
    echo "EXODUS_GUEST_RESULT=$rc"
    /sbin/shutdown -p now
    exit "$rc"
}
trap finish EXIT
echo EXODUS_BOOT_BEGIN
uname -a
test "$(uname -s)" = FreeBSD
test "$(uname -p)" = aarch64
freebsd-version -ku
sysctl hw.model hw.ncpu hw.physmem
df -h /
gpart show
kenv smbios.system.product || :
service sshd onestop || :
sysrc sshd_enable=NO
echo EXODUS_FREEBSD_BOOT_PASS

#!/bin/sh
# Disposable FreeBSD guest only. No external Linux binaries or credentials.
set -eu
export PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin
exec >/dev/console 2>&1
finish() {
    rc=$?
    trap - EXIT
    echo "EXODUS_GUEST_RESULT=$rc"
    /sbin/shutdown -p now
    exit "$rc"
}
trap finish EXIT
test "$(uname -s)" = FreeBSD
test "$(uname -p)" = aarch64
uname -a
sysctl hw.model
service sshd onestop || :
sysrc sshd_enable=NO
work=$(mktemp -d /var/tmp/tumor-linux-abi.XXXXXX)
mkdir "$work/seed"
seed=
for d in /dev/msdosfs/[cC][iI][dD][aA][tT][aA]; do
    [ -e "$d" ] || continue
    seed=$d
    break
done
test -n "$seed"
mount -t msdosfs -o ro "$seed" "$work/seed"
for f in linux-armv8 linux-lse; do
    cp "$work/seed/$f" "$work/$f"
    chmod 755 "$work/$f"
done
umount "$work/seed"
chmod 755 "$work"
service linux onestart
kldstat
sysctl compat.linux.osrelease
file "$work/linux-armv8" "$work/linux-lse"
brandelf -t Linux "$work/linux-armv8" "$work/linux-lse"
pw useradd -n exodusref -m -s /bin/sh -w no
out=$(su -m exodusref -c "$work/linux-armv8")
test "$out" = TUMOR_LINUX_ABI_OK
echo EXODUS_LINUX_ARM64_SYSCALL_PASS
# LSE is not present on Cortex-A57. Prove this independently of Steam.
set +e
su -m exodusref -c "ulimit -c 0; $work/linux-lse"
rc=$?
set -e
echo "EXODUS_LSE_PROBE_RESULT=$rc"
test "$rc" -eq 132
echo EXODUS_LINUX_ABI_REFERENCE_PASS

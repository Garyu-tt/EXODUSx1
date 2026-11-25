#!/bin/sh
# Disposable FreeBSD VM; build only, never load the module.
set -eu
export PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin
exec >/dev/console 2>&1
finish() {
    rc=$?
    trap - EXIT
    echo "EXODUS_MAILBOX_BUILD_RESULT=$rc"
    /sbin/shutdown -p now
    exit "$rc"
}
trap finish EXIT
test "$(uname -s)" = FreeBSD
test "$(uname -p)" = aarch64
test "$(freebsd-version -k)" = 15.1-RELEASE
service sshd onestop || :
sysrc sshd_enable=NO
work=$(mktemp -d /var/tmp/tumor-mailbox.XXXXXX)
mkdir "$work/seed" "$work/module" "$work/source"
seed=
for d in /dev/msdosfs/[cC][iI][dD][aA][tT][aA]; do
    [ -e "$d" ] || continue
    seed=$d
    break
done
test -n "$seed"
mount -t msdosfs -o ro "$seed" "$work/seed"
cp "$work/seed/tumor_mailbox.c" "$work/seed/Makefile" "$work/module/"
umount "$work/seed"
base=https://download.freebsd.org/releases/arm64/aarch64/15.1-RELEASE
fetch -o "$work/MANIFEST" "$base/MANIFEST"
fetch -o "$work/src.txz" "$base/src.txz"
hash=$(awk '$1 == "src.txz" {print $2}' "$work/MANIFEST")
test "$(printf %s "$hash" | wc -c)" -eq 64
test "$(sha256 -q "$work/src.txz")" = "$hash"
echo "EXODUS_MAILBOX_SRC_SHA256=$hash"
tar -xJf "$work/src.txz" -C "$work/source"
sysdir="$work/source/usr/src/sys"
test -f "$sysdir/conf/kmod.mk"
make -C "$work/module" SYSDIR="$sysdir" all
test -s "$work/module/tumor_mailbox.ko"
file "$work/module/tumor_mailbox.ko"
readelf -h "$work/module/tumor_mailbox.ko" | grep -F AArch64
readelf -h "$work/module/tumor_mailbox.ko" | grep -F REL
sha256 "$work/module/tumor_mailbox.ko"
if kldstat -n tumor_mailbox.ko >/dev/null 2>&1; then exit 1; fi
echo EXODUS_MAILBOX_NATIVE_BUILD_PASS

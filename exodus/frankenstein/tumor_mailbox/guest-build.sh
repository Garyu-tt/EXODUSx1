#!/bin/sh
# Disposable stock FreeBSD VM; build and load/unload with no mailbox node.
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
test "$hash" = cf5762da53fd52e1eaf0f9ceee9bf58cbe314c821031d0d9ffa76823185a89e1
test "$(sha256 -q "$work/src.txz")" = "$hash"
echo "EXODUS_MAILBOX_SRC_SHA256=$hash"
tar -xJf "$work/src.txz" -C "$work/source"
sysdir="$work/source/usr/src/sys"
test -f "$sysdir/conf/kmod.mk"
make -C "$work/module" SYSDIR="$sysdir" all
test -s "$work/module/tumor_mailbox.ko"
file "$work/module/tumor_mailbox.ko"
readelf -h "$work/module/tumor_mailbox.ko" | grep -F AArch64
# arm64 bsd.kmod.mk links the intermediate REL .kld into a DYN .ko.
readelf -h "$work/module/tumor_mailbox.ko" | grep -E 'Type:[[:space:]]+DYN'
readelf -h "$work/module/tumor_mailbox.ko" | grep -E 'Class:[[:space:]]+ELF64'
readelf -h "$work/module/tumor_mailbox.ko" | grep -F 'little endian'
readelf -SW "$work/module/tumor_mailbox.ko" | grep -F set_modmetadata_set
sha256 "$work/module/tumor_mailbox.ko"
# Stock QEMU virt has no Tumor reference FDT or shared-memory node.
# Verify absence of an attached driver before and after registration.
if kldstat -n tumor_mailbox.ko >/dev/null 2>&1; then exit 1; fi
if devinfo | grep -Eq '(^|[[:space:]])tumor_mailbox[0-9]'; then exit 1; fi
if [ "$(sysctl -n hw.fdt.model 2>/dev/null || true)" = "Tumor Bao QEMU reference" ]; then exit 1; fi
for attempt in 1 2; do
    kldload "$work/module/tumor_mailbox.ko"
    kldstat -n tumor_mailbox.ko
    kldstat -m ofwbus/tumor_mailbox
    if devinfo | grep -Eq '(^|[[:space:]])tumor_mailbox[0-9]'; then exit 1; fi
    kldunload tumor_mailbox.ko
    if kldstat -n tumor_mailbox.ko >/dev/null 2>&1; then exit 1; fi
    if kldstat -m ofwbus/tumor_mailbox >/dev/null 2>&1; then exit 1; fi
    echo "EXODUS_MAILBOX_LOAD_UNLOAD_PASS=$attempt"
done
echo EXODUS_MAILBOX_NO_DEVICE_PASS
export_dir=$(mktemp -d /var/tmp/tumor-mailbox-out.XXXXXX)
mount -t msdosfs /dev/msdosfs/EXODUSOUT "$export_dir"
cp "$work/module/tumor_mailbox.ko" "$export_dir/"
(cd "$export_dir" && sha256 -r tumor_mailbox.ko > SHA256SUMS)
sync
umount "$export_dir"
rmdir "$export_dir"
echo EXODUS_MAILBOX_NATIVE_BUILD_PASS

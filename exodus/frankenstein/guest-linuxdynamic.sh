#!/bin/sh
# Disposable FreeBSD reference VM only; not a system-wide install procedure.
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
service sshd onestop || :
sysrc sshd_enable=NO
work=$(mktemp -d /var/tmp/tumor-dynamic.XXXXXX)
chmod 755 "$work"
mkdir "$work/seed"
seed=
for d in /dev/msdosfs/[cC][iI][dD][aA][tT][aA]; do
    [ -e "$d" ] || continue
    seed=$d
    break
done
test -n "$seed"
mount -t msdosfs -o ro "$seed" "$work/seed"
for f in linux-dynamic ld-linux-aarch64.so.1 libc.so.6; do
    cp "$work/seed/$f" "$work/$f"
done
umount "$work/seed"
service linux onestart
emul=$(sysctl -n compat.linux.emul_path)
test "$emul" = /compat/linux
install -d "$emul/lib/aarch64-linux-gnu"
install -m 755 "$work/ld-linux-aarch64.so.1" "$emul/lib/ld-linux-aarch64.so.1"
install -m 755 "$work/libc.so.6" "$emul/lib/aarch64-linux-gnu/libc.so.6"
chmod 755 "$work/linux-dynamic"
file "$work/linux-dynamic" "$emul/lib/ld-linux-aarch64.so.1" "$emul/lib/aarch64-linux-gnu/libc.so.6"
brandelf -t Linux "$work/linux-dynamic"
pw useradd -n exodusref -m -s /bin/sh -w no
# Record the output in a file independently of su status; require all gates.
install -o exodusref -g exodusref -m 600 /dev/null "$work/probe.log"
su -m exodusref -c "ulimit -c 0; $work/linux-dynamic > $work/probe.log 2>&1; rc=\$?; echo TUMOR_GLIBC_EXIT=\$rc >> $work/probe.log; exit \$rc" || :
cat "$work/probe.log"
grep -q '^TUMOR_GLIBC_START version=' "$work/probe.log"
grep -q '^TUMOR_GLIBC_SYSCALLS_OK sys=Linux ' "$work/probe.log"
grep -Fx TUMOR_GLIBC_DLOPEN_OK "$work/probe.log"
grep -Fx TUMOR_GLIBC_PTHREAD_OK "$work/probe.log"
grep -Fx TUMOR_GLIBC_EXIT=0 "$work/probe.log"
echo EXODUS_LINUX_DYNAMIC_PASS

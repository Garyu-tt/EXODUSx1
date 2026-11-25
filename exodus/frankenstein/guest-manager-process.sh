#!/bin/sh
# Isolated native process-provider acceptance, independent of the full daemon.
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
freebsd-version -ku
service sshd onestop || :
sysrc sshd_enable=NO
export ASSUME_ALWAYS_YES=yes
pkg bootstrap -y
pkg update -f
pkg install -y git-lite rust
work=$(mktemp -d /var/tmp/exodus-manager-process.XXXXXX)
mkdir "$work/seed" "$work/upstream" "$work/probe"
seed=
for d in /dev/msdosfs/[cC][iI][dD][aA][tT][aA]; do
    [ -e "$d" ] || continue
    seed=$d
    break
done
test -n "$seed"
mount -t msdosfs -o ro "$seed" "$work/seed"
cp "$work/seed/bsd_process.rs" "$work/seed/test-bsd-process.sh" "$work/probe/"
umount "$work/seed"
git -C "$work/upstream" init
git -C "$work/upstream" fetch --depth=1 https://github.com/evlav/steamos-manager.git a0d399defebd60db30ba38325d9cf47dbda49bee
test "$(git -C "$work/upstream" rev-parse FETCH_HEAD)" = a0d399defebd60db30ba38325d9cf47dbda49bee
git -C "$work/upstream" show FETCH_HEAD:Cargo.lock > "$work/probe/Cargo.lock"
test "$(git hash-object "$work/probe/Cargo.lock")" = e077c31929cb202ea244c3ba63c8a26482e69d94
# No root-only process visibility: run the native probe as a disposable user.
pw useradd exodusproc -m -s /bin/sh
chmod 0711 "$work"
chown -R exodusproc "$work/probe"
su -l exodusproc -c "test \"\$(id -u)\" -ne 0 && env PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin sh '$work/probe/test-bsd-process.sh' '$work/probe/bsd_process.rs' '$work/probe/Cargo.lock'"
echo EXODUS_MANAGER_PROCESS_UNPRIVILEGED_PASS

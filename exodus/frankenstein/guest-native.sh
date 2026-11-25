#!/bin/sh
# Run only in disposable FreeBSD ARM64 guest.
set -eu
# nuageinit inherits the base-system rc PATH, not a login shell PATH.
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
service sshd onestop || :
sysrc sshd_enable=NO
# Mount the disposable NoCloud volume containing the exact checkout sources.
work=$(mktemp -d /var/tmp/exodus-native.XXXXXX)
mkdir "$work/seed" "$work/src"
seed=
for d in /dev/msdosfs/[cC][iI][dD][aA][tT][aA]; do
    [ -e "$d" ] || continue
    seed=$d
    break
done
test -n "$seed"
mount -t msdosfs -o ro "$seed" "$work/seed"
for f in sessiond.c sessiond-dbus.inc test_sessiond.py test_sessiond_dbus.sh; do
    cp "$work/seed/$f" "$work/src/$f"
done
mkdir "$work/src/exodus" "$work/src/tests"
for f in task_center.py task_center_child.py; do
    cp "$work/seed/$f" "$work/src/exodus/$f"
done
for f in test_task_center.py test_task_center_child.py; do
    cp "$work/seed/$f" "$work/src/tests/$f"
done
touch "$work/src/exodus/__init__.py" "$work/src/tests/__init__.py"
umount "$work/seed"
export ASSUME_ALWAYS_YES=yes
pkg bootstrap -y
pkg update -f
pkg install -y python3 glib dbus pkgconf
command -v pkgconf
cd "$work/src"
cc -std=c11 -Wall -Wextra -Werror -O2 sessiond.c -o exodus-sessiond
cc -std=c11 -Wall -Wextra -Werror -O2 -DEXODUS_DBUS sessiond.c $(pkgconf --cflags --libs gio-2.0) -o exodus-sessiond-dbus
file exodus-sessiond exodus-sessiond-dbus
sha256 exodus-sessiond exodus-sessiond-dbus
pw useradd -n exodusref -m -s /bin/sh -w no
# All test code and binaries remain root-owned, readable and executable.
chmod 755 "$work" "$work/src"
su -m exodusref -c "cd '$work/src' && /usr/local/bin/python3 test_sessiond.py ./exodus-sessiond"
echo EXODUS_NATIVE_SESSION_PASS
# Execute the exact accepted R03 tests as a non-root user on real FreeBSD.
# The POSIX capability check is mandatory: a skipped test is not acceptance.
su -m exodusref -c "cd '$work/src' && /usr/local/bin/python3 -c 'import os; assert hasattr(os, \"posix_spawn\") and hasattr(os, \"WCONTINUED\")'"
su -m exodusref -c "cd '$work/src' && /usr/local/bin/python3 -m unittest -v tests.test_task_center tests.test_task_center_child"
echo EXODUS_TASK_CENTER_FREEBSD_PASS
su -m exodusref -c "cd '$work/src' && /usr/local/bin/dbus-run-session -- sh test_sessiond_dbus.sh ./exodus-sessiond-dbus"
export_dir=$(mktemp -d /var/tmp/exodus-export.XXXXXX)
mount -t msdosfs /dev/msdosfs/EXODUSOUT "$export_dir"
tar -czf "$export_dir/exodus-native.tar.gz" exodus-sessiond exodus-sessiond-dbus
pkg query -a '%n-%v %o' | sort > "$export_dir/dependencies.txt"
(cd "$export_dir" && sha256 -r exodus-native.tar.gz dependencies.txt > SHA256SUMS)
sync
umount "$export_dir"
rmdir "$export_dir"
echo EXODUS_NATIVE_DBUS_PASS

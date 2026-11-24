#!/bin/sh
# Runs only inside the disposable FreeBSD ARM64 VM.
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
echo EXODUS_GUEST_BEGIN
uname -a
test "$(uname -s)" = FreeBSD
test "$(uname -m)" = arm64
test "$(uname -p)" = aarch64
freebsd-version -ku
sysctl hw.model hw.ncpu hw.physmem
# The reference CI image permits empty-password SSH by default.
# No host port is forwarded; disable its daemon before doing build work.
service sshd onestop || :
sysrc sshd_enable=NO
export ASSUME_ALWAYS_YES=yes
pkg bootstrap -y
pkg update -f
pkg -vv
pkg info
# A real source build (not merely installation of a precompiled Wayland).
pkg install -y git-lite meson ninja pkgconf expat libffi libxml2
command -v git meson ninja pkgconf
git init /usr/ports
git -C /usr/ports remote add origin https://github.com/freebsd/freebsd-ports.git
git -C /usr/ports fetch --depth=1 origin 98796bd539ea8f03adb7c9d6f9f3a3d54dcca553
git -C /usr/ports checkout --detach FETCH_HEAD
test "$(git -C /usr/ports rev-parse HEAD)" = 98796bd539ea8f03adb7c9d6f9f3a3d54dcca553
for port in graphics/wayland graphics/mesa-libs graphics/mesa-dri graphics/vulkan-loader graphics/libdrm x11-servers/xwayland x11/libinput multimedia/pipewire x11-wm/gamescope x11/plasma6-plasma security/openssh-portable games/linux-steam-utils emulators/wine-proton; do
    echo "EXODUS_PORT=$port"
    for v in PKGNAME IGNORE ONLY_FOR_ARCHS NOT_FOR_ARCHS BUILD_DEPENDS LIB_DEPENDS RUN_DEPENDS; do
        printf '%s=' "$v"
        make -C "/usr/ports/$port" -V "$v"
    done
done
make -C /usr/ports/graphics/wayland BATCH=yes USE_PACKAGE_DEPENDS=yes package
wayland_package=$(make -C /usr/ports/graphics/wayland -V PKGFILE)
test -s "$wayland_package"
sha256 "$wayland_package"
echo EXODUS_WAYLAND_SOURCE_BUILD_PASS
# Run an actual Valve Gamescope source package build against the BSD port.
# Dependencies may be installed from the binary repository; the target may not.
echo EXODUS_GAMESCOPE_BEGIN
port=/usr/ports/x11-wm/gamescope
test -z "$(make -C "$port" -V IGNORE)"
make -C "$port" BATCH=yes USE_PACKAGE_DEPENDS=yes all-depends-list
make -C "$port" BATCH=yes USE_PACKAGE_DEPENDS=yes checksum
make -C "$port" BATCH=yes USE_PACKAGE_DEPENDS=yes package
package=$(make -C "$port" -V PKGFILE)
test -s "$package"
sha256 "$package"
pkg add -f "$package"
pkg info -d gamescope
file /usr/local/bin/gamescope
/usr/local/bin/gamescope --help
pkg info
export_dir=$(mktemp -d /var/tmp/exodus-export.XXXXXX)
mount -t msdosfs /dev/msdosfs/EXODUSOUT "$export_dir"
cp "$wayland_package" "$export_dir/wayland.pkg"
cp "$package" "$export_dir/gamescope.pkg"
pkg query -a '%n-%v %o' | sort > "$export_dir/dependencies.txt"
(cd "$export_dir" && sha256 -r wayland.pkg gamescope.pkg dependencies.txt > SHA256SUMS)
sync
umount "$export_dir"
rmdir "$export_dir"
echo EXODUS_GAMESCOPE_SOURCE_BUILD_PASS

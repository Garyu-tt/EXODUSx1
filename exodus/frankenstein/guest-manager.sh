#!/bin/sh
# Execute in the disposable FreeBSD guest, never on a real console.
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
echo EXODUS_MANAGER_GUEST_BEGIN
input_patch=/var/tmp/steamos-manager-freebsd-input.patch
cat > "$input_patch" <<'EXODUS_INPUT_PATCH'
__EXODUS_MANAGER_INPUT_PATCH__
EXODUS_INPUT_PATCH
test "$(sha256 -q "$input_patch")" = 1a20c9f9d73f1e09cd863fb70937ce4ab361fc8732b42602e54a06ed8d9ab571
platform_patch=/var/tmp/steamos-manager-freebsd-platform.patch
cat > "$platform_patch" <<'EXODUS_PLATFORM_PATCH'
__EXODUS_MANAGER_PLATFORM_PATCH__
EXODUS_PLATFORM_PATCH
test "$(sha256 -q "$platform_patch")" = b80c87119555153103148629bbdc8b5029b6d97d1f481f814bdb568c3746861e
uname -a
test "$(uname -s)" = FreeBSD
test "$(uname -m)" = arm64
test "$(uname -p)" = aarch64
freebsd-version -ku
service sshd onestop || :
sysrc sshd_enable=NO
export ASSUME_ALWAYS_YES=yes
pkg bootstrap -y
pkg update -f
pkg install -y git-lite rust pkgconf dbus glib speech-dispatcher llvm19 libudev-devd
# Rust bindgen needs a shared libclang, not just the base clang executable.
# The pinned ports tree uses LLVM 19 and installs libclang here.
export LIBCLANG_PATH=/usr/local/llvm19/lib
test -r "$LIBCLANG_PATH/libclang.so"
test -r /usr/local/include/speech-dispatcher/libspeechd.h
export BINDGEN_EXTRA_CLANG_ARGS="-I/usr/local/include -I/usr/local/include/speech-dispatcher"
export LIBRARY_PATH="/usr/local/lib"
echo EXODUS_MANAGER_LIBCLANG_READY
for tool in cc git patch rustc cargo sha256 timeout; do command -v "$tool"; done
rustc -Vv
cargo -V
pkg info
mkdir -p /var/tmp/exodus-manager
cd /var/tmp/exodus-manager
git init
git remote add origin https://github.com/evlav/steamos-manager.git
git fetch --depth=1 origin a0d399defebd60db30ba38325d9cf47dbda49bee
git checkout --detach FETCH_HEAD
test "$(git rev-parse HEAD)" = a0d399defebd60db30ba38325d9cf47dbda49bee
test -f Cargo.lock
test "$(sha256 -q Cargo.lock)" = 001bf56bb9f357710986bea869c67473e2afd358bf2288a00b661ba8ea0027c1
test "$(sha256 -q steamos-manager/Cargo.toml)" = 78a3b78831c6c3a2834d167b2bfdbb166363b65efc082cc5e449e57a46277937
test "$(sha256 -q steamos-manager/src/bin/steamosctl.rs)" = 78806f38c4d30810516913e13dfdb48e06483d53920d6263a098373d6ddfb3c2
test "$(sha256 -q steamos-manager/src/cec.rs)" = ed02ae2ed3ba58eecdda89526afef542a7a2787fc376b20a609ec7a60e19b4c9
test "$(sha256 -q steamos-manager/src/hardware.rs)" = 7c6c6a6d700ee0c5ddb950d5bbf779b393714b14fe02e937a35b75d24d220a24
test "$(sha256 -q steamos-manager/src/lib.rs)" = 80f3710c44535fba54864e0f7230afad4fa67f0e16853e25d7e0b228ef05fc51
test "$(sha256 -q steamos-manager/src/manager/user.rs)" = 7db6bc290d6160399d94f15be7ebc1d9e2b76c18ed2f41ea2a56ffdddf0ac447
test "$(sha256 -q steamos-manager/src/power.rs)" = bb8d86cb0638f4a58fedb643653f63ef3b3bb68d4fae84516b41e9fcc1ab2623
test ! -e steamos-manager/examples/freebsd_inotify_probe.rs
test ! -e steamos-manager/src/cec_types.rs
git apply --check "$platform_patch"
git apply "$platform_patch"
test "$(sha256 -q Cargo.lock)" = 6035e3ec787fbad93d3dfb45f2ebc93d9b53139484cd572603ca8b1e1220da2e
test "$(sha256 -q steamos-manager/Cargo.toml)" = db05e269748b1c4d884e1d1fa51437e641218296b9df10525f2e0e99dcb159af
test "$(sha256 -q steamos-manager/examples/freebsd_inotify_probe.rs)" = 01ddfeff92c34d0d24f9de262e81ffa392c0bc482a3113ce0641a04d8b1178b7
test "$(sha256 -q steamos-manager/src/bin/steamosctl.rs)" = fa04dff9eab2ef582bcfea3fa8c172d6440d75e367785c0b5ef83abaaace4863
test "$(sha256 -q steamos-manager/src/cec.rs)" = 556bcb3aa3ed1c331a346e4df8ae42bac9d6489d2e0b32ae60e2eeaf163b0d7d
test "$(sha256 -q steamos-manager/src/cec_types.rs)" = f22b37ed144bc19c3f02bf7c6b3e793f4761992555366377cf684038ef7115b9
test "$(sha256 -q steamos-manager/src/hardware.rs)" = 72f8977c3f4e8fc427445700876259efbce42c4231fe6a1c4b5292e26bc483c7
test "$(sha256 -q steamos-manager/src/lib.rs)" = b67d72f586dc8e9f54d7531142e37eab0e6407ace6d8d5f91f823d23f3555543
test "$(sha256 -q steamos-manager/src/manager/user.rs)" = 9c93d5a40fa7c810110191b9293163d7edfb9ee954f59958a631adcc0548c30a
test "$(sha256 -q steamos-manager/src/power.rs)" = 80347e3238e72a7d731a027fd847c5d0b8228be1ecedd3dfe40ca87a17732f2d
echo EXODUS_MANAGER_FREEBSD_PLATFORM_PATCH_PASS
cargo fetch --locked
cargo_home=${CARGO_HOME:-"$HOME/.cargo"}
set -- "$cargo_home"/registry/src/*/input-linux-sys-0.9.0
test "$#" -eq 1
test -d "$1"
input_sys_dir=$1
set -- "$cargo_home"/registry/src/*/input-linux-0.7.1
test "$#" -eq 1
test -d "$1"
input_linux_dir=$1
registry_root=$(dirname "$input_sys_dir")
test "$(dirname "$input_linux_dir")" = "$registry_root"
test "$(sha256 -q "$input_sys_dir/src/input.rs")" = e1a14a5fb13c2900df9065126b21b2aa0b803c1e7d95349b34fd2df37de944c4
test "$(sha256 -q "$input_sys_dir/src/uinput.rs")" = 4773765ef58720a1343d937d306acdc80a9029761fa6ebd395ad537f774a747e
test "$(sha256 -q "$input_linux_dir/src/evdev.rs")" = 4c8c9aeaa750990684685833a23191f155089c0727979c9277965421c0994c4f
test "$(sha256 -q "$input_linux_dir/src/macros.rs")" = caa32d0238b1a7903a071e58610fc85a69ca094ef46eb478ea1e876ecfcdae25
test "$(sha256 -q "$input_linux_dir/src/uinput.rs")" = f835460c74d09976ac081067688ffa77f713300a8e921db40cea64f6ab0760c8
test ! -e "$input_sys_dir/src/freebsd.rs"
# This patch supports the FreeBSD uinput operations used by steamos-manager.
# Other input-linux helpers are outside that runtime claim; mismatched
# Linux-only ioctl encodings stay unavailable.
patch -d "$registry_root" -p1 < "$input_patch"
test "$(sha256 -q "$input_sys_dir/src/input.rs")" = b5e04123a2083c4fc7c71949f2d1bc6d99a3137489cacaf7832952bbe1f8930e
test "$(sha256 -q "$input_sys_dir/src/uinput.rs")" = 5aad07c3495bc285f146c5d47617631e168bebec182a20051a4f5aa1a5a384b9
test "$(sha256 -q "$input_sys_dir/src/freebsd.rs")" = 7a5fe5d71004fbe9f69935d1fdd1579e1386be75e26c190f68773c74974af131
test "$(sha256 -q "$input_linux_dir/src/evdev.rs")" = 68cdf000ac6b523ca1293bed15236ec82457380e86b68e3536c38c5da17306fa
test "$(sha256 -q "$input_linux_dir/src/macros.rs")" = 6f202c72bb420c242a5775afc0435e4cb23f16f9602f89f65aff85a9953bde0f
test "$(sha256 -q "$input_linux_dir/src/uinput.rs")" = a65615795b8f9b137ad3c23af876056b02bfd5679e1a16e1117b94a4810b2293
echo EXODUS_MANAGER_FREEBSD_INPUT_PATCH_PASS

cat > /var/tmp/exodus-manager-input-abi.c <<'EOF'
#include <stddef.h>
#include <dev/evdev/input.h>
#include <dev/evdev/uinput.h>

_Static_assert(sizeof(struct input_event) == 24, "input_event ABI");
_Static_assert(sizeof(struct input_id) == 8, "input_id ABI");
_Static_assert(sizeof(struct input_absinfo) == 24, "input_absinfo ABI");
_Static_assert(sizeof(struct input_keymap_entry) == 40, "input_keymap_entry ABI");
_Static_assert(sizeof(struct ff_periodic_effect) == 32, "ff_periodic_effect ABI");
_Static_assert(sizeof(struct ff_effect) == 48, "ff_effect ABI");
_Static_assert(offsetof(struct ff_effect, u) == 16, "ff_effect union offset");
_Static_assert(sizeof(struct uinput_setup) == 92, "uinput_setup ABI");
_Static_assert(sizeof(struct uinput_abs_setup) == 28, "uinput_abs_setup ABI");
_Static_assert(sizeof(struct uinput_ff_upload) == 104, "uinput_ff_upload ABI");

int main(void) { return 0; }
EOF
cc -std=c11 -Wall -Wextra -Werror /var/tmp/exodus-manager-input-abi.c -o /var/tmp/exodus-manager-input-abi
/var/tmp/exodus-manager-input-abi
echo EXODUS_MANAGER_FREEBSD_INPUT_ABI_PASS
cargo tree --locked -p steamos-manager > /var/tmp/exodus-manager-freebsd-tree.txt
grep -F 'inotify v0.11.4' /var/tmp/exodus-manager-freebsd-tree.txt
grep -F 'inotify-sys v0.1.8' /var/tmp/exodus-manager-freebsd-tree.txt
if grep -E 'linux-cec(-sys)? v' /var/tmp/exodus-manager-freebsd-tree.txt; then
    echo 'Linux-only CEC crates leaked into the FreeBSD dependency tree' >&2
    exit 1
fi
echo EXODUS_MANAGER_FREEBSD_DEPENDENCY_TREE_PASS
# Build original MIT-licensed protocol/proc-macro crates, not replacements.
# Do not use --workspace: the full daemon still needs platform adapters.
cargo build --locked -p steamos-manager-proxy -p steamos-manager-macros
find target/debug -maxdepth 1 -name 'libsteamos_manager*' -exec file {} \;
echo EXODUS_MANAGER_PROTOCOL_BUILD_PASS
# Compile the pinned upstream daemon with the scoped FreeBSD platform patch.
# Successful check/build/probes are not full daemon runtime proof; any failure remains a blocker.
set +e
cargo check --locked -p steamos-manager
manager_rc=$?
if [ "$manager_rc" -eq 0 ]; then
    cargo build --locked -p steamos-manager
    manager_rc=$?
fi
if [ "$manager_rc" -eq 0 ]; then
    echo EXODUS_MANAGER_DAEMON_BUILD_PASS
    cargo build --locked -p steamos-manager --example freebsd_inotify_probe
    manager_rc=$?
fi
if [ "$manager_rc" -eq 0 ]; then
    timeout 15s target/debug/examples/freebsd_inotify_probe
    manager_rc=$?
fi
set -e
echo "EXODUS_MANAGER_DAEMON_CHECK_RESULT=$manager_rc"
echo EXODUS_MANAGER_AUDIT_COMPLETE

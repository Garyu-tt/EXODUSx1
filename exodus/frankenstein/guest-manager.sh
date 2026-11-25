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
pkg install -y git-lite rust pkgconf dbus glib speech-dispatcher llvm19
# Rust bindgen needs a shared libclang, not just the base clang executable.
# The pinned ports tree uses LLVM 19 and installs libclang here.
export LIBCLANG_PATH=/usr/local/llvm19/lib
test -r "$LIBCLANG_PATH/libclang.so"
test -r /usr/local/include/speech-dispatcher/libspeechd.h
export BINDGEN_EXTRA_CLANG_ARGS="-I/usr/local/include -I/usr/local/include/speech-dispatcher"
export LIBRARY_PATH="/usr/local/lib"
echo EXODUS_MANAGER_LIBCLANG_READY
for tool in cc git patch rustc cargo sha256; do command -v "$tool"; done
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
# Build original MIT-licensed protocol/proc-macro crates, not replacements.
# Do not use --workspace: the full daemon still needs platform adapters.
cargo build --locked -p steamos-manager-proxy -p steamos-manager-macros
find target/debug -maxdepth 1 -name 'libsteamos_manager*' -exec file {} \;
echo EXODUS_MANAGER_PROTOCOL_BUILD_PASS
# Collect real compiler diagnostics for the untouched upstream daemon.
# A successful check is not runtime proof; failure remains a port blocker.
set +e
cargo check --locked -p steamos-manager
manager_rc=$?
set -e
echo "EXODUS_MANAGER_DAEMON_CHECK_RESULT=$manager_rc"
echo EXODUS_MANAGER_AUDIT_COMPLETE

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
for tool in git rustc cargo; do command -v "$tool"; done
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

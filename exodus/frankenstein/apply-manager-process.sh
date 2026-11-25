#!/bin/sh
# Apply only to the pinned upstream file. Preserve Linux and its fixture tests.
set -eu
if [ "$#" -ne 2 ]; then exit 2; fi
root=$1
provider=$2
src="$root/steamos-manager/src/lib.rs"
test "$(git hash-object "$src")" = 17e607bff42d47c6cf9986130eb4ad3cbf927c6f
test ! -e "$root/steamos-manager/src/bsd_process.rs"
sed -e '/^pub(crate) fn read_comm(pid: u32)/i\
#[cfg(any(not(target_os = "freebsd"), test))]
' -e '/^pub(crate) fn get_appid(pid: u32)/i\
#[cfg(any(not(target_os = "freebsd"), test))]
' "$src" > "$src.tmp"
cat >> "$src.tmp" <<'RS'

#[cfg(target_os = "freebsd")]
mod bsd_process;
#[cfg(all(target_os = "freebsd", not(test)))]
pub(crate) use bsd_process::{get_appid, read_comm};
RS
mv "$src.tmp" "$src"
cp "$provider" "$root/steamos-manager/src/bsd_process.rs"
git -C "$root" diff --check

#!/bin/sh
# Exercise the exact BSD provider independently of the full manager daemon.
set -eu
if [ "$#" -ne 2 ]; then exit 2; fi
provider=$1
upstream_lock=$2
work=$(mktemp -d /var/tmp/tumor-process-test.XXXXXX)
trap 'rm -rf "$work"' EXIT
mkdir "$work/src"
cp "$provider" "$work/src/lib.rs"
cat > "$work/Cargo.toml" <<'TOML'
[package]
name = "tumor-bsd-process-probe"
version = "0.0.0"
edition = "2024"
[dependencies]
anyhow = "=1.0.102"
libc = "=0.2.186"
TOML
# Preserve the exact upstream registry package hashes in an isolated lock.
cat > "$work/Cargo.lock" <<'LOCK'
version = 4

[[package]]
name = "anyhow"
version = "1.0.102"
source = "registry+https://github.com/rust-lang/crates.io-index"
checksum = "7f202df86484c868dbad7eaa557ef785d5c66295e41b460ef922eca0723b842c"

[[package]]
name = "libc"
version = "0.2.186"
source = "registry+https://github.com/rust-lang/crates.io-index"
checksum = "68ab91017fe16c622486840e4c83c9a37afeff978bd239b5293d61ece587de66"

[[package]]
name = "tumor-bsd-process-probe"
version = "0.0.0"
dependencies = [
 "anyhow",
 "libc",
]
LOCK
# Refuse dependency drift before launching Cargo.
for record in \
    'checksum = "7f202df86484c868dbad7eaa557ef785d5c66295e41b460ef922eca0723b842c"' \
    'checksum = "68ab91017fe16c622486840e4c83c9a37afeff978bd239b5293d61ece587de66"'; do
    grep -Fx "$record" "$upstream_lock" >/dev/null
done
cat >> "$work/src/lib.rs" <<'RS'

#[cfg(test)]
mod isolated_probe {
    use super::*;
    use std::process::Command;
    #[test]
    fn child_appid() {
        let mut child = Command::new("/bin/sleep").arg("30")
            .env_clear().env("SteamGameId", "1234567").spawn().unwrap();
        let result = get_appid(child.id());
        let _ = child.kill();
        let _ = child.wait();
        assert_eq!(result.unwrap(), Some(1234567));
    }
}
RS
# Keep this isolated build bounded even if the dependency registry stalls.
(cd "$work" && timeout 20m cargo test --locked --lib -- --test-threads=1)
echo EXODUS_MANAGER_BSD_PROCESS_TEST_PASS

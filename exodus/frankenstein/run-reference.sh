#!/bin/sh
# Linux build host. All changes are confined to a temporary VM.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=$(realpath -m "${1:-frankenstein-results}")
mkdir -p "$out"
mode=${2:-graphics}
case "$mode" in
    graphics) guest=guest-build.sh; gate=EXODUS_GAMESCOPE_SOURCE_BUILD_PASS ;;
    manager) guest=guest-manager.sh; gate=EXODUS_MANAGER_AUDIT_COMPLETE ;;
    ssh) guest=guest-ssh.sh; gate=EXODUS_SSH_REFERENCE_PASS ;;
    boot) guest=guest-boot.sh; gate=EXODUS_FREEBSD_BOOT_PASS ;;
    native) guest=guest-native.sh; gate=EXODUS_NATIVE_DBUS_PASS ;;
    linuxabi) guest=guest-linuxabi.sh; gate=EXODUS_LINUX_ABI_REFERENCE_PASS ;;
    linuxdynamic) guest=guest-linuxdynamic.sh; gate=EXODUS_LINUX_DYNAMIC_PASS ;;
    resident) guest=guest-resident.sh; gate=EXODUS_RESIDENT_SSH_DBUS_PASS ;;
    rcresident) guest=guest-rc-resident.sh; gate=EXODUS_RC_RESIDENT_PASS ;;
    *) echo "Unknown reference mode" >&2; exit 2 ;;
esac
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"
base=https://download.freebsd.org/releases/CI-IMAGES/15.1-RELEASE/aarch64/Latest
image=FreeBSD-15.1-RELEASE-arm64-aarch64-BASIC-CI-ufs.raw.xz
curl -fL --retry 3 "$base/CHECKSUM.SHA256" -o "$out/CHECKSUM.SHA256"
curl -fL --retry 3 "$base/$image" -o "$image"
hash=$(awk -v name="$image" '$1=="SHA256" && $2=="(" name ")" && $3=="=" {print $4}' "$out/CHECKSUM.SHA256")
test "${#hash}" -eq 64
printf '%s  %s\n' "$hash" "$image" | sha256sum -c -
xz -dc "$image" > disk.raw
rm "$image"
qemu-img resize -f raw disk.raw 32G
truncate -s 16M seed.img
mkfs.vfat -n CIDATA seed.img
# Separate output volume; no host directory or physical device is exposed.
truncate -s 128M export.img
mkfs.vfat -n EXODUSOUT export.img
printf 'instance-id: exodus-frankenstein-%s\nlocal-hostname: exodus-reference\n' "$mode" > meta-data
if [ "$mode" = manager ]; then
    python3 "$here/prepare_manager_user_data.py" \
        "$here/$guest" \
        "$here/patches/steamos-manager-freebsd-input.patch" \
        "$here/patches/steamos-manager-freebsd-platform.patch" \
        user-data
else
    cp "$here/$guest" user-data
fi
mcopy -i seed.img meta-data user-data ::
if [ "$mode" = linuxabi ]; then
    test "$(uname -m)" = aarch64
    cc -nostdlib -static -march=armv8-a -Wl,--build-id=none "$here/linux-abi-probe.S" -o linux-armv8
    cc -nostdlib -static -march=armv8.1-a -DTEST_LSE -Wl,--build-id=none "$here/linux-abi-probe.S" -o linux-lse
    readelf -h -n linux-armv8 linux-lse > "$out/linux-probe-elf.txt"
    mcopy -i seed.img linux-armv8 linux-lse ::
fi
if [ "$mode" = linuxdynamic ]; then
    test "$(uname -m)" = aarch64
    cc -std=c11 -Wall -Wextra -Werror -O2 -march=armv8-a -mno-outline-atomics "$here/linux-dynamic-probe.c" -pthread -ldl -o linux-dynamic
    readelf -h -l -d linux-dynamic > "$out/linux-dynamic-elf.txt"
    grep -F '/lib/ld-linux-aarch64.so.1' "$out/linux-dynamic-elf.txt"
    cp -L /lib/ld-linux-aarch64.so.1 ld-linux-aarch64.so.1
    cp -L /lib/aarch64-linux-gnu/libc.so.6 libc.so.6
    sha256sum linux-dynamic ld-linux-aarch64.so.1 libc.so.6 > "$out/linux-dynamic-SHA256SUMS"
    dpkg-query -W libc6 > "$out/glibc-version.txt"
    ./linux-dynamic | tee "$out/linux-dynamic-host.txt"
    grep -Fx TUMOR_GLIBC_PTHREAD_OK "$out/linux-dynamic-host.txt"
    mcopy -i seed.img linux-dynamic ld-linux-aarch64.so.1 libc.so.6 ::
fi
if [ "$mode" = rcresident ]; then
    for f in sessiond.c sessiond-dbus.inc tumor_reference_bus tumor_reference_session; do mcopy -i seed.img "$here/$f" ::; done
fi
if [ "$mode" = resident ]; then
    for f in sessiond.c sessiond-dbus.inc; do mcopy -i seed.img "$here/$f" ::; done
fi
if [ "$mode" = native ]; then
    for f in sessiond.c sessiond-dbus.inc test_sessiond.py test_sessiond_dbus.sh; do
        mcopy -i seed.img "$here/$f" ::
    done
    # Carry exactly the accepted Task Center modules and tests, not an installed package.
    for f in exodus/task_center.py exodus/task_center_child.py tests/test_task_center.py tests/test_task_center_child.py; do
        mcopy -i seed.img "$here/../../$f" ::
    done
fi
# Emulate the Tegra CPU ISA baseline; no KVM/host CPU assumption.
efi=/usr/share/qemu-efi-aarch64/QEMU_EFI.fd
test -f "$efi"
# The FreeBSD nuageinit rc script mounts its seed without -o ro.
# Use a disposable writable copy; no host files are exposed to the guest.
limit=145m
[ "$mode" != ssh ] || limit=65m
[ "$mode" != native ] || limit=65m
[ "$mode" != resident ] || limit=65m
[ "$mode" != rcresident ] || limit=65m
[ "$mode" != boot ] || limit=12m
[ "$mode" != linuxabi ] || limit=12m
[ "$mode" != linuxdynamic ] || limit=12m
# Stream serial evidence even if the guest never reaches the build script.
# Preserve QEMU/timeout status independently of log forwarding.
: > "$out/serial.log"
tail -n +1 -F "$out/serial.log" &
log_pid=$!
trap 'kill "$log_pid" 2>/dev/null || :; rm -rf "$work"' EXIT
rc=0
timeout "$limit" qemu-system-aarch64 \
    -machine virt -cpu cortex-a57 -accel tcg,thread=multi -smp 4 -m 4096 \
    -bios "$efi" -display none -monitor none -serial "file:$out/serial.log" -no-reboot \
    -drive if=none,file=disk.raw,format=raw,id=system \
    -device virtio-blk-device,drive=system \
    -drive if=none,file=seed.img,format=raw,id=seed \
    -device virtio-blk-device,drive=seed \
    -drive if=none,file=export.img,format=raw,id=output \
    -device virtio-blk-device,drive=output \
    -netdev user,id=net -device virtio-net-device,netdev=net || rc=$?
kill "$log_pid" 2>/dev/null || :
wait "$log_pid" 2>/dev/null || :
if [ "$rc" -ne 0 ]; then
    echo "QEMU exit=$rc; final guest serial output:" >&2
    tail -n 80 "$out/serial.log" >&2
    exit "$rc"
fi
grep -F "$gate" "$out/serial.log"
grep -F 'EXODUS_GUEST_RESULT=0' "$out/serial.log"
if [ "$mode" = native ]; then
    tr -d '\r' < "$out/serial.log" | grep -Fx EXODUS_TASK_CENTER_FREEBSD_PASS
fi

# The upstream manager workload deliberately preserves compiler diagnostics
# and exits cleanly even when the full daemon does not compile. A completed
# audit is not a successful port: enforce its explicit result at the host.
if [ "$mode" = manager ]; then
    tr -d '\r' < "$out/serial.log" > "$out/manager-evidence.txt"
    grep -Fx EXODUS_MANAGER_PROTOCOL_BUILD_PASS "$out/manager-evidence.txt"
    grep -Fx EXODUS_MANAGER_FREEBSD_PLATFORM_PATCH_PASS "$out/manager-evidence.txt"
    grep -Fx EXODUS_MANAGER_FREEBSD_DEPENDENCY_TREE_PASS "$out/manager-evidence.txt"
    grep -Fx EXODUS_MANAGER_DAEMON_BUILD_PASS "$out/manager-evidence.txt"
    grep -Fx EXODUS_MANAGER_INOTIFY_RUST_PASS "$out/manager-evidence.txt"
    # Command substitution strips trailing newlines: count records before
    # accepting their value, including empty duplicate result records.
    if ! awk '
        /^EXODUS_MANAGER_DAEMON_CHECK_RESULT=/ {
            count++
            if ($0 != "EXODUS_MANAGER_DAEMON_CHECK_RESULT=0") invalid=1
        }
        END { exit !(count == 1 && !invalid) }
    ' "$out/manager-evidence.txt"; then
        printf 'Upstream manager daemon compile gate not passed (expected exactly one result=0)\n' >&2
        exit 1
    fi
    grep -Fx EXODUS_MANAGER_EXECUTABLE_SMOKE_PASS "$out/manager-evidence.txt"
    grep -Fx EXODUS_MANAGER_ARTIFACT_GUEST_PASS "$out/manager-evidence.txt"
    echo EXODUS_MANAGER_DAEMON_COMPILE_PASS
fi

# A fresh emulator process, same disk, no cloud-init seed or output drive.
# A successful first-stage shutdown alone is not evidence of cold boot.
if [ "$mode" = rcresident ]; then
    tr -d '\r' < "$out/serial.log" | grep -Fx EXODUS_RC_COLD_BOOT_PREPARED
    cold_log="$out/cold-serial.log"
    : > "$cold_log"
    tail -n +1 -F "$cold_log" &
    log_pid=$!
    rc=0
    timeout 12m qemu-system-aarch64 \
        -machine virt -cpu cortex-a57 -accel tcg,thread=multi -smp 4 -m 4096 \
        -bios "$efi" -display none -monitor none -serial "file:$cold_log" -no-reboot \
        -drive if=none,file=disk.raw,format=raw,id=system \
        -device virtio-blk-device,drive=system \
        -netdev user,id=net -device virtio-net-device,netdev=net || rc=$?
    kill "$log_pid" 2>/dev/null || :
    wait "$log_pid" 2>/dev/null || :
    if [ "$rc" -ne 0 ]; then
        echo "Cold boot QEMU exit=$rc" >&2
        tail -n 80 "$cold_log" >&2
        exit "$rc"
    fi
    # FreeBSD console output can use CRLF.
    tr -d '\r' < "$cold_log" > "$out/cold-evidence.txt"
    grep -Fx EXODUS_RC_COLD_BOOT_BEGIN "$out/cold-evidence.txt"
    grep -Fx EXODUS_RC_COLD_BOOT_PASS "$out/cold-evidence.txt"
    grep -Fx EXODUS_COLD_GUEST_RESULT=0 "$out/cold-evidence.txt"
fi

# Extract only known filenames after successful guest shutdown and validation.
case "$mode" in
    graphics) files="wayland.pkg gamescope.pkg dependencies.txt" ;;
    manager) files="steamos-manager steamosctl steamos-manager.ldd.txt steamosctl.ldd.txt steamos-manager.help.txt steamosctl.help.txt steamosctl.version.txt dependencies.txt freebsd-version.txt source-revision.txt cargo-tree.txt" ;;
    native) files="exodus-native.tar.gz dependencies.txt" ;;
    rcresident) files="tumor-resident-reference.tar.gz dependencies.txt" ;;
    *) exit 0 ;;
esac
mkdir extracted
mcopy -i export.img ::/SHA256SUMS extracted/guest-SHA256SUMS
# Admit the complete manifest before copying any payload. A malformed,
# duplicate or unexpected record must not be silently filtered out.
awk -v files="$files" '
    BEGIN {
        n = split(files, names, " ")
        for (i = 1; i <= n; i++) expected[names[i]] = 1
    }
    {
        if (NF != 2 || length($1) != 64 || $1 ~ /[^0-9a-fA-F]/ ||
            !($2 in expected) || seen[$2]++) {
            bad = 1
            next
        }
        digest[$2] = tolower($1)
        count++
    }
    END {
        if (bad || count != n) exit 1
        for (i = 1; i <= n; i++) printf "%s  %s\n", digest[names[i]], names[i]
    }
' extracted/guest-SHA256SUMS > extracted/SHA256SUMS
for f in $files; do
    mcopy -i export.img "::/$f" "extracted/$f"
    test -s "extracted/$f"
done
(cd extracted && sha256sum -c --strict SHA256SUMS)
if [ "$mode" = rcresident ]; then
    # Verify the archive itself, without extracting guest-controlled paths.
    python3 - extracted/tumor-resident-reference.tar.gz extracted/dependencies.txt <<'PY'
import hashlib
import pathlib
import re
import struct
import sys
import tarfile

expected = {
    "bin/exodus-sessiond-dbus", "bin/exodus-session-select",
    "rc.d/tumor_reference_bus", "rc.d/tumor_reference_session",
    "dependencies.txt", "freebsd-version.txt", "README.txt", "SHA256SUMS",
}
directories = {".", "bin", "rc.d"}
payload = {}
seen = set()
with tarfile.open(sys.argv[1], "r:gz") as archive:
    for member in archive:
        name = member.name.removeprefix("./").rstrip("/") or "."
        if name in seen:
            raise SystemExit(f"duplicate archive member: {name}")
        seen.add(name)
        if member.isdir() and name in directories:
            continue
        if not member.isfile() or name not in expected:
            raise SystemExit(f"unexpected archive member: {name}")
        if member.size > 32 * 1024 * 1024:
            raise SystemExit(f"oversized archive member: {name}")
        payload[name] = archive.extractfile(member).read()
        if name.startswith(("bin/", "rc.d/")) and not member.mode & 0o111:
            raise SystemExit(f"non-executable archive member: {name}")
if set(payload) != expected:
    raise SystemExit("reference archive contents incomplete")
manifest = {}
for line in payload["SHA256SUMS"].decode("ascii").splitlines():
    match = re.fullmatch(r"([0-9a-fA-F]{64})  ?(.+)", line)
    if not match:
        raise SystemExit("invalid internal checksum record")
    digest, name = match.groups()
    if name in manifest or name not in expected - {"SHA256SUMS"}:
        raise SystemExit(f"unexpected checksum entry: {name}")
    manifest[name] = digest.lower()
if set(manifest) != expected - {"SHA256SUMS"}:
    raise SystemExit("internal checksum coverage incomplete")
for name, digest in manifest.items():
    if hashlib.sha256(payload[name]).hexdigest() != digest:
        raise SystemExit(f"internal checksum mismatch: {name}")
if payload["dependencies.txt"] != pathlib.Path(sys.argv[2]).read_bytes():
    raise SystemExit("dependency manifest differs from exported copy")
elf = payload["bin/exodus-sessiond-dbus"]
if len(elf) < 64 or elf[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", elf, 18)[0] != 183:
    raise SystemExit("session daemon is not little-endian ELF64 AArch64")
print("EXODUS_RC_ARCHIVE_CONTENT_PASS")
PY
fi
if [ "$mode" = manager ]; then
    python3 - extracted <<'PY'
import pathlib
import struct
import sys
root = pathlib.Path(sys.argv[1])
for name in ("steamos-manager", "steamosctl"):
    path = root / name
    if not 64 <= path.stat().st_size <= 48 * 1024 * 1024:
        raise SystemExit(f"invalid executable size: {name}")
    with path.open("rb") as stream:
        elf = stream.read(64)
    if (elf[:6] != b"\x7fELF\x02\x01"
            or struct.unpack_from("<H", elf, 18)[0] != 183
            or struct.unpack_from("<H", elf, 16)[0] not in (2, 3)):
        raise SystemExit(f"not an executable ELF64 AArch64: {name}")
if (root / "source-revision.txt").read_text().strip() != "a0d399defebd60db30ba38325d9cf47dbda49bee":
    raise SystemExit("unexpected manager source revision")
print("EXODUS_MANAGER_ARTIFACT_HOST_PASS")
PY
fi
mkdir -p "$out/packages"
cp extracted/* "$out/packages/"
echo EXODUS_ARTIFACT_EXPORT_PASS

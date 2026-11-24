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
    linuxabi) guest=guest-linuxabi.sh; gate=EXODUS_LINUX_ABI_REFERENCE_PASS ;;
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
truncate -s 4M seed.img
mkfs.vfat -n CIDATA seed.img
# Separate output volume; no host directory or physical device is exposed.
truncate -s 128M export.img
mkfs.vfat -n EXODUSOUT export.img
printf 'instance-id: exodus-frankenstein-%s\nlocal-hostname: exodus-reference\n' "$mode" > meta-data
cp "$here/$guest" user-data
mcopy -i seed.img meta-data user-data ::
if [ "$mode" = linuxabi ]; then
    test "$(uname -m)" = aarch64
    cc -nostdlib -static -march=armv8-a -Wl,--build-id=none "$here/linux-abi-probe.S" -o linux-armv8
    cc -nostdlib -static -march=armv8.1-a -DTEST_LSE -Wl,--build-id=none "$here/linux-abi-probe.S" -o linux-lse
    readelf -h -n linux-armv8 linux-lse > "$out/linux-probe-elf.txt"
    mcopy -i seed.img linux-armv8 linux-lse ::
fi
if [ "$mode" = native ]; then
    for f in sessiond.c sessiond-dbus.inc test_sessiond.py test_sessiond_dbus.sh; do
        mcopy -i seed.img "$here/$f" ::
    done
fi
if [ "$mode" = linuxabi ]; then
    cc -march=armv8-a -nostdlib -static -no-pie "$here/linux-abi-probe.S" -o linux-armv8
    cc -march=armv8.1-a+lse -DTEST_LSE -nostdlib -static -no-pie "$here/linux-abi-probe.S" -o linux-lse
    mcopy -i seed.img linux-armv8 linux-lse ::
fi
# Emulate the Tegra CPU ISA baseline; no KVM/host CPU assumption.
efi=/usr/share/qemu-efi-aarch64/QEMU_EFI.fd
test -f "$efi"
# The FreeBSD nuageinit rc script mounts its seed without -o ro.
# Use a disposable writable copy; no host files are exposed to the guest.
limit=145m
[ "$mode" != ssh ] || limit=65m
[ "$mode" != native ] || limit=65m
[ "$mode" != boot ] || limit=12m
[ "$mode" != linuxabi ] || limit=12m
[ "$mode" != linuxabi ] || limit=12m
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

# Extract only known filenames after successful guest shutdown and validation.
case "$mode" in
    graphics) files="wayland.pkg gamescope.pkg dependencies.txt" ;;
    native) files="exodus-native.tar.gz dependencies.txt" ;;
    *) exit 0 ;;
esac
mkdir extracted
mcopy -i export.img ::/SHA256SUMS extracted/guest-SHA256SUMS
: > extracted/SHA256SUMS
for f in $files; do
    mcopy -i export.img "::/$f" "extracted/$f"
    test -s "extracted/$f"
    sum=$(awk -v name="$f" '$2==name && NF==2 {print $1}' extracted/guest-SHA256SUMS)
    test "${#sum}" -eq 64
    printf '%s\n' "$sum" | grep -Eq '^[0-9a-fA-F]{64}$'
    printf '%s  %s\n' "$sum" "$f" >> extracted/SHA256SUMS
done
(cd extracted && sha256sum -c --strict SHA256SUMS)
mkdir -p "$out/packages"
cp extracted/* "$out/packages/"
echo EXODUS_ARTIFACT_EXPORT_PASS

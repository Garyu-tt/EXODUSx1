#!/bin/sh
# Standalone narrow compile gate, no Bao or physical device boot.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=$(realpath -m "$1")
mkdir -p "$out"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"
base=https://download.freebsd.org/releases/CI-IMAGES/15.1-RELEASE/aarch64/Latest
image=FreeBSD-15.1-RELEASE-arm64-aarch64-BASIC-CI-ufs.raw.xz
curl -fL --retry 3 "$base/CHECKSUM.SHA256" -o "$out/CHECKSUM.SHA256"
curl -fL --retry 3 "$base/$image" -o "$image"
hash=$(awk -v name="$image" '$1=="SHA256" && $2=="(" name ")" && $3=="=" {print $4}' "$out/CHECKSUM.SHA256")
test "$(printf %s "$hash" | wc -c)" -eq 64
printf '%s  %s\n' "$hash" "$image" | sha256sum -c -
xz -dc "$image" > disk.raw
qemu-img resize -f raw disk.raw 16G
truncate -s 16M seed.img
mkfs.vfat -n CIDATA seed.img
truncate -s 16M export.img
mkfs.vfat -n EXODUSOUT export.img
printf 'instance-id: exodus-mailbox-build\nlocal-hostname: exodus-reference\n' > meta-data
cp "$here/guest-build.sh" user-data
mcopy -i seed.img meta-data user-data "$here/tumor_mailbox.c" "$here/Makefile" ::
rc=0
timeout 35m qemu-system-aarch64 -machine virt -cpu cortex-a57 -accel tcg,thread=multi -smp 4 -m 4096 \
    -bios /usr/share/qemu-efi-aarch64/QEMU_EFI.fd -display none -monitor none -serial "file:$out/serial.log" -no-reboot \
    -drive if=none,file=disk.raw,format=raw,id=system -device virtio-blk-device,drive=system \
    -drive if=none,file=seed.img,format=raw,id=seed -device virtio-blk-device,drive=seed \
    -drive if=none,file=export.img,format=raw,id=output -device virtio-blk-device,drive=output \
    -netdev user,id=net -device virtio-net-device,netdev=net || rc=$?
cat "$out/serial.log"
test "$rc" -eq 0
tr -d '\r' < "$out/serial.log" > "$out/normalized.log"
test "$(grep -c '^EXODUS_MAILBOX_NATIVE_BUILD_PASS$' "$out/normalized.log")" -eq 1
test "$(grep -c '^EXODUS_MAILBOX_BUILD_RESULT=0$' "$out/normalized.log")" -eq 1
for attempt in 1 2; do
    test "$(grep -c "^EXODUS_MAILBOX_LOAD_UNLOAD_PASS=$attempt$" "$out/normalized.log")" -eq 1
done
test "$(grep -c '^EXODUS_MAILBOX_NO_DEVICE_PASS$' "$out/normalized.log")" -eq 1

mkdir "$out/module"
mcopy -i export.img ::/tumor_mailbox.ko ::/SHA256SUMS "$out/module/"
python3 - "$out/module" <<'PY'
import hashlib
import pathlib
import re
import struct
import sys
root = pathlib.Path(sys.argv[1])
p = root / "tumor_mailbox.ko"
if not 64 <= p.stat().st_size <= 1024 * 1024:
    raise SystemExit("invalid module size")
data = p.read_bytes()
if data[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<HH", data, 16) != (3, 183):
    raise SystemExit("expected ELF64 little-endian AArch64 ET_DYN")
m = re.fullmatch(r"([0-9a-f]{64}) [ *]?tumor_mailbox\.ko\n?", (root / "SHA256SUMS").read_text())
if not m or hashlib.sha256(data).hexdigest() != m.group(1):
    raise SystemExit("module checksum mismatch")
print("EXODUS_MAILBOX_ARTIFACT_HOST_PASS")
PY

#!/bin/bash
# Isolated QEMU-only positive attachment control. Stock kernel, one synthetic
# peer and one FreeBSD guest; never touch mapped contents or enable coherence.
set -euo pipefail
test "$#" -eq 3
case "$3" in
    mapping|roundtrip) mode=$3 ;;
    *) exit 2 ;;
esac
src=$(cd "$(dirname "$0")/.." && pwd)
module_dir=$(realpath "$1")
evidence=$(realpath -m "$2")
mkdir -p "$evidence"
(cd "$module_dir" && sha256sum -c SHA256SUMS)
test -s "$module_dir/tumor_mailbox.ko"
readelf -h "$module_dir/tumor_mailbox.ko" | grep -F AArch64
readelf -h "$module_dir/tumor_mailbox.ko" | grep -E 'Type:[[:space:]]+DYN'
out=$(mktemp -d)
trap 'rm -rf "$out"' EXIT
mkdir "$out/config" "$out/images"
git init "$out/bao"
git -C "$out/bao" remote add origin https://github.com/bao-project/bao-hypervisor.git
git -C "$out/bao" fetch --depth=1 origin 0af4a1ab558ad60c9658af4f43756c4536dd3141
git -C "$out/bao" checkout --detach FETCH_HEAD
test "$(git -C "$out/bao" rev-parse HEAD)" = 0af4a1ab558ad60c9658af4f43756c4536dd3141
# Minimal reference BL33; QEMU virt only. No physical target payload.
printf '.text\n.global _start\n_start:\n mov x0, #0x50000000\n br x0\n' > "$out/ref-bl33.S"
aarch64-linux-gnu-gcc -ffreestanding -c "$out/ref-bl33.S" -o "$out/ref-bl33.o"
aarch64-linux-gnu-objcopy -O binary -j .text "$out/ref-bl33.o" "$out/ref-bl33.bin"
git init "$out/tfa"
git -C "$out/tfa" remote add origin https://github.com/bao-project/arm-trusted-firmware.git
git -C "$out/tfa" fetch --depth=1 origin e4ffdd76c3c4b1488eb9c290e5b5c676f68f6b38
git -C "$out/tfa" checkout --detach FETCH_HEAD
test "$(git -C "$out/tfa" rev-parse HEAD)" = e4ffdd76c3c4b1488eb9c290e5b5c676f68f6b38
make -C "$out/tfa" -j2 PLAT=qemu CROSS_COMPILE=aarch64-linux-gnu- QEMU_USE_GIC_DRIVER=QEMU_GICV3 BL33="$out/ref-bl33.bin" bl1 fip
cp "$out/tfa/build/qemu/release/bl1.bin" "$out/ref-flash.bin"
dd if="$out/tfa/build/qemu/release/fip.bin" of="$out/ref-flash.bin" seek=64 bs=4096 conv=notrunc status=none
curl --fail --location --retry 2 --max-time 180 -o "$out/kernel.txz" https://download.freebsd.org/releases/arm64/aarch64/15.1-RELEASE/kernel.txz
printf '%s  %s\n' 05f770c6af035fab312cd65592f1dbd1bff020c7daa75d8d4ecc96cae8f042c0 "$out/kernel.txz" | sha256sum -c -
mkdir "$out/freebsd"
member="$(tar -tJf "$out/kernel.txz" | awk '$0 == "boot/kernel/kernel" || $0 == "./boot/kernel/kernel" { print }')"
test "$(printf '%s\n' "$member" | wc -l)" -eq 1
test -n "$member"
tar -xJf "$out/kernel.txz" -C "$out/freebsd" "$member"
kernel="$out/freebsd/boot/kernel/kernel"
printf '%s  %s\n' e66c772fb7b1144c61735c18dceb1f598f87f18b5acd9781c1d78327a6f68379 "$kernel" | sha256sum -c -

release=https://download.freebsd.org/releases/arm64/aarch64/15.1-RELEASE
curl --fail --location --retry 2 --max-time 60 -o "$out/MANIFEST" "$release/MANIFEST"
# Kernel independently pinned above. Match the manifest archive hash too.
test "$(awk '$1 == "kernel.txz" {print $2}' "$out/MANIFEST")" = 05f770c6af035fab312cd65592f1dbd1bff020c7daa75d8d4ecc96cae8f042c0
base_hash=$(awk '$1 == "base.txz" {print $2}' "$out/MANIFEST")
printf '%s\n' "$base_hash" | grep -Ex '[a-f0-9]{64}'
curl --fail --location --retry 2 --max-time 300 -o "$out/base.txz" "$release/base.txz"
printf '%s  %s\n' "$base_hash" "$out/base.txz" | sha256sum -c -
mkdir -p "$out/base" "$out/root/sbin" "$out/root/bin" "$out/root/rescue" "$out/root/dev" "$out/root/etc" "$out/root/boot/kernel"
tar -tJf "$out/base.txz" | awk '/^(\.\/)?rescue\// { print }' > "$out/rescue-members"
test -s "$out/rescue-members"
tar -xJf "$out/base.txz" -C "$out/base" --no-same-owner --no-recursion -T "$out/rescue-members"
for tool in init sh sysctl kldload kldstat kldunload; do
    test -f "$out/base/rescue/$tool"
    test ! -L "$out/base/rescue/$tool"
    cmp "$out/base/rescue/init" "$out/base/rescue/$tool"
done
install -m 0755 "$out/base/rescue/init" "$out/root/sbin/init"
ln "$out/root/sbin/init" "$out/root/bin/sh"
for tool in sysctl kldload kldstat kldunload; do ln "$out/root/sbin/init" "$out/root/rescue/$tool"; done
install -m 0644 "$module_dir/tumor_mailbox.ko" "$out/root/boot/kernel/tumor_mailbox.ko"
install -m 0755 "$src/tumor_mailbox/bao-mapping-rc.sh" "$out/root/etc/rc"
: > "$out/root/etc/ttys"
makefs -t ffs -o version=1 -B little -s 64m "$out/root.ufs" "$out/root"
sh "$src/tumor_mailbox/prepare-mapping-fdt.sh" 2 "$out/mapping.dtb"
peer="$src/bao_guest.S"
if [ "$mode" = roundtrip ]; then
    fdtput "$out/mapping.dtb" /mailbox@f0000000 exodus,roundtrip-control
    fdtget "$out/mapping.dtb" /mailbox@f0000000 exodus,roundtrip-control >/dev/null
    peer="$src/tumor_mailbox/bao-roundtrip-peer.S"
fi
clang --target=aarch64-none-elf -march=armv8-a -ffreestanding -DGUEST_A -c "$peer" -o "$out/peer.o"
ld.lld --oformat=binary -T "$src/bao_guest.ld" "$out/peer.o" -o "$out/images/guest-A.bin"
clang --target=aarch64-none-elf -march=armv8-a -ffreestanding -DTUMOR_FREEBSD_NATIVE_METADATA -c "$src/bao_freebsd_boot.S" -o "$out/boot.o"
ld.lld --oformat=binary -T "$src/bao_guest.ld" "$out/boot.o" -o "$out/boot.bin"
python3 "$src/pack_bao_freebsd_root.py" "$kernel" "$out/boot.bin" "$out/mapping.dtb" "$out/root.ufs" "$out/images/guest-B.bin"
cp "$src/bao_freebsd_reference.c" "$out/config/tumor_mapping.c"
make -C "$out/bao" -j2 CROSS_COMPILE=aarch64-linux-gnu- PLATFORM=qemu-aarch64-virt CONFIG_REPO="$out/config" CONFIG=tumor_mapping CPPFLAGS="-DTUMOR_IMAGES=$out/images"
bao="$out/bao/bin/qemu-aarch64-virt/tumor_mapping/bao.bin"
sha256sum "$out/root.ufs" "$out/mapping.dtb" "$bao" "$module_dir/tumor_mailbox.ko" > "$evidence/input-sha256.txt"
rc=0
timeout 150s qemu-system-aarch64 -machine virt,secure=on,virtualization=on,gic-version=3 -cpu cortex-a53 -smp 4 -m 1024 -nodefaults -nic none -display none -monitor none -serial stdio -no-reboot -bios "$out/ref-flash.bin" -device "loader,file=$bao,addr=0x50000000,force-raw=on" > "$evidence/serial.log" 2>&1 || rc=$?
cat "$evidence/serial.log"
test "$rc" -eq 124
tr -d '\r' < "$evidence/serial.log" > "$evidence/normalized.log"
for marker in FREEBSD_HANDOFF_AFTER_PEER_IPC MAPPING_ONLY_DRIVER_ATTACHED MAPPING_ONLY_DRIVER_DETACHED; do
    test "$(grep -ac "^TUMOR_BAO_REF: $marker$" "$evidence/normalized.log")" -eq 1
done
if grep -aiE 'panic:|fatal exception|TMR_COHERENT_MAPPING_ATTACHED|TMR_ROUNDTRIP_FAILED' "$evidence/normalized.log"; then exit 1; fi
if [ "$mode" = mapping ]; then
    test "$(grep -ac 'tumor_mailbox0: TMR_MAPPING_ONLY_ATTACHED; no shared-memory access performed' "$evidence/normalized.log")" -eq 1
    if grep -aF TMR_ROUNDTRIP_CONTROL_PASS "$evidence/normalized.log"; then exit 1; fi
    echo EXODUS_BAO_MAPPING_ONLY_PASS
else
    test "$(grep -ac '^tumor_mailbox0: TMR_ROUNDTRIP_CONTROL_PASS' "$evidence/normalized.log")" -eq 1
    echo EXODUS_BAO_ROUNDTRIP_PASS
fi

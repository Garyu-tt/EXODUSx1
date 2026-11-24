#!/bin/sh
set -eu
mode=${2:-native}
case "$mode" in native) test -n "$DEVKITPRO";; host) :;; *) exit 2;; esac
export EXR_BUILD_MODE="$mode"
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
mkdir -p "$1"
out=$(CDPATH= cd -- "$1" && pwd)
cd "$work"
archive=mbedtls-3.6.8.tar.bz2
curl --fail --location --retry 2 --max-time 180 --proto '=https' --proto-redir '=https' "https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.8/$archive" -o "$archive"
printf '%s  %s\n' bc54b818e87f49c0b3281dd85d24adaede0e63b4e822e3fbd49c005a38b9595b "$archive" | sha256sum -c -
tar -xjf "$archive"
cd mbedtls-3.6.8
python3 - <<'PY'
from pathlib import Path
import re, os
p=Path('include/mbedtls/mbedtls_config.h')
s=p.read_text()
for macro in ('MBEDTLS_NET_C','MBEDTLS_TIMING_C',
              'MBEDTLS_SSL_SESSION_TICKETS','MBEDTLS_SSL_TICKET_C',
              'MBEDTLS_SSL_CACHE_C','MBEDTLS_SSL_CONTEXT_SERIALIZATION',
              'MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_ENABLED',
              'MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL_ENABLED',
              'MBEDTLS_PSA_CRYPTO_STORAGE_C','MBEDTLS_PSA_ITS_FILE_C'):
    s,n=re.subn(r'^#define '+macro+r'[ \t]*$', '// #define '+macro,s,flags=re.M)
    assert n==1,macro
for macro in (('MBEDTLS_ENTROPY_HARDWARE_ALT','MBEDTLS_NO_PLATFORM_ENTROPY','MBEDTLS_PLATFORM_MS_TIME_ALT') if os.environ['EXR_BUILD_MODE']=='native' else ()):
    s,n=re.subn(r'^//\s*#define '+macro+r'[ \t]*$', '#define '+macro,s,flags=re.M)
    assert n==1,macro
p.write_text(s)
PY
if [ "$mode" = host ]; then
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF -DUSE_SHARED_MBEDTLS_LIBRARY=OFF \
    -DUSE_STATIC_MBEDTLS_LIBRARY=ON -DCMAKE_INSTALL_PREFIX="$work/install" -DCMAKE_INSTALL_LIBDIR=lib
  cmake --build build --parallel 2
  cmake --install build
  sh "$src/generate_tls_fixtures.sh" "$work/tls_fixtures.h"
  lib="$work/install/lib"
  cc -std=c11 -Wall -Wextra -Werror -I"$work" -I"$work/install/include" \
    "$src/host_mtls_smoke.c" -Wl,--start-group \
    "$lib/libmbedtls.a" "$lib/libmbedx509.a" "$lib/libmbedcrypto.a" \
    "$lib/libeverest.a" "$lib/libp256m.a" -Wl,--end-group -pthread -o "$out/host_mtls_smoke"
  "$out/host_mtls_smoke"
  exit 0
fi
cc="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-gcc"
arch="-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE"
cmake -S . -B build -DCMAKE_SYSTEM_NAME=Generic \
  -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY -DCMAKE_C_COMPILER="$cc" \
  -DCMAKE_C_FLAGS="$arch -D__SWITCH__" -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF -DUSE_SHARED_MBEDTLS_LIBRARY=OFF \
  -DUSE_STATIC_MBEDTLS_LIBRARY=ON -DCMAKE_INSTALL_PREFIX="$work/install" -DCMAKE_INSTALL_LIBDIR=lib
cmake --build build --parallel 2
cmake --install build
lib="$work/install/lib"
# Mbed TLS exports these third-party targets as PUBLIC dependencies of
# mbedcrypto. A manual static link must preserve that dependency closure.
for archive in libmbedtls.a libmbedx509.a libmbedcrypto.a libeverest.a libp256m.a; do
    test -f "$lib/$archive"
done
nx="$DEVKITPRO/libnx"
sh "$src/generate_tls_fixtures.sh" "$work/tls_fixtures.h"
"$cc" -std=gnu11 -Wall -Wextra -Werror $arch -D__SWITCH__ \
  -I"$work" -I"$work/install/include" -I"$nx/include" -specs="$nx/switch.specs" \
  "$src/native_tls13_smoke.c" -Wl,--start-group \
  "$lib/libmbedtls.a" "$lib/libmbedx509.a" "$lib/libmbedcrypto.a" \
  "$lib/libeverest.a" "$lib/libp256m.a" -Wl,--end-group \
  -L"$nx/lib" -lnx -o "$out/exodus-tls13-smoke.elf"
nacptool --create "EXODUS TLS 1.3 Smoke" "EXODUSx1" "0.3.16" "$out/smoke.nacp"
elf2nro "$out/exodus-tls13-smoke.elf" "$out/exodus-tls13-smoke.nro" --icon="$nx/default_icon.jpg" --nacp="$out/smoke.nacp"
"$DEVKITPRO/devkitA64/bin/aarch64-none-elf-readelf" -h "$out/exodus-tls13-smoke.elf" | grep AArch64
test "$(dd if="$out/exodus-tls13-smoke.nro" bs=1 skip=16 count=4 2>/dev/null)" = NRO0
(cd "$out" && sha256sum exodus-tls13-smoke.nro > SHA256SUMS)

#!/bin/sh
set -eu
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
d=$(mktemp -d)
trap 'rm -rf "$d"' EXIT HUP INT TERM
cd "$src"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
for test in test_remote_gate test_horizon_provider test_remote_wire; do
    cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -no-pie "$test.c" -o "$d/$test"
    "$d/$test"
done
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -no-pie -D__SWITCH__ -Itest_stubs test_horizon_provider.c -o "$d/provider_mock"
"$d/provider_mock"
sh test_remote_tls.sh
sh test_remote_session.sh
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -no-pie loopback_server.c -o "$d/server" $(pkg-config --cflags --libs openssl)
python3 test_loopback.py "$d/server"
printf '%s\n' 'PASS: complete host admission suite (not a device test)'

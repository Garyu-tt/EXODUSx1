#!/usr/bin/env bash
# Compile inventory: no target execution, no Linux-only adapters.
set -euo pipefail
donor=${1:?donor checkout}; generated=${2:?generated grammar}; out=${3:?output directory}
: "${DEVKITPRO:?devkitPro required}"
cc="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-gcc"
nm="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-nm"
readelf="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-readelf"
libnx="$DEVKITPRO/libnx"
mkdir -p "$out/objects"
flags=(-std=gnu11 -funsigned-char -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE -D__SWITCH__ -DNO_HISTORY -DSHELL -DNXBSD_AST_BRIDGE -Werror=implicit-function-declaration -include exodus/terminal/freebsd_horizon_compile.h -I"$libnx/include" -I"$donor/bin/sh" -I"$donor/contrib/libedit" -I"$generated")
"$cc" --version > "$out/toolchain.txt"
printf 'module\tcompile\truntime\n' > "$out/modules.tsv"
objects=(); failed=0
probe() {
    local name=$1 source=$2 obj="$out/objects/$1.o"
    if "$cc" "${flags[@]}" -c "$source" -o "$obj" > "$out/$name.log" 2>&1; then
        objects+=("$obj")
        "$nm" -u "$obj" > "$out/$name.undefined.txt"
        printf '%s\tyes\tnot_tested\n' "$name" >> "$out/modules.tsv"
    else
        failed=$((failed+1))
        printf '%s\tno\tnot_tested\n' "$name" >> "$out/modules.tsv"
        head -n 12 "$out/$name.log"
    fi
}
for m in syntax nodes builtins; do probe "generated-$m" "$generated/$m.c"; done
for m in parser eval memalloc mystring alias output var expand input redir error options jobs exec trap main cd miscbltin mail arith_yacc arith_yylex histedit; do
    probe "$m" "$donor/bin/sh/$m.c"
done
probe echo "$donor/bin/sh/bltin/echo.c"
probe printf "$donor/usr.bin/printf/printf.c"
probe test "$donor/bin/test/test.c"
probe kill "$donor/bin/kill/kill.c"
donor_compiled=${#objects[@]}
donor_failed=$failed
probe target-stdio exodus/terminal/freebsd_stdio_horizon.c
for m in ast_bridge pipe fd context task pipeline commands; do
    probe "target-$m" "exodus/terminal/nxbsd_$m.c"
done
cat "$out/modules.tsv"
if ((${#objects[@]})); then
    "$cc" -r "${objects[@]}" -o "$out/shell-partial.o"
    "$nm" -u "$out/shell-partial.o" > "$out/shell-partial.undefined.txt"
    cat "$out/shell-partial.undefined.txt"
    "$readelf" -h "$out/shell-partial.o" > "$out/elf-header.txt"
fi
link=not_attempted
if ((failed==0)); then
    link=no
    if "$cc" -specs="$libnx/switch.specs" "$out/shell-partial.o" -L"$libnx/lib" -lnx -o "$out/shell-probe.elf" > "$out/final-link.log" 2>&1; then
        link=yes
        "$readelf" -h "$out/shell-probe.elf" > "$out/final-elf-header.txt"
        "$nm" -u "$out/shell-probe.elf" > "$out/final-undefined.txt"
    else
        cat "$out/final-link.log"
    fi
fi
printf 'donor_compiled=%s\ndonor_failed=%s\ncompiled=%s\nfailed=%s\nfinal_link=%s\nruntime=not_tested\n' "$donor_compiled" "$donor_failed" "${#objects[@]}" "$failed" "$link" | tee "$out/summary.txt"
# A successful inventory does not mean the shell builds. Require a real target object.
test -s "$out/objects/generated-syntax.o"
test -s "$out/objects/target-stdio.o"
test -s "$out/elf-header.txt"
grep -q AArch64 "$out/elf-header.txt"

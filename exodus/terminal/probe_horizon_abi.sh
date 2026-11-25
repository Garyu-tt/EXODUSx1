#!/usr/bin/env bash
# Compile/link only. Never execute these probes on the Switch.
set -euo pipefail
out=${1:?output directory required}
mkdir -p "$out"
: "${DEVKITPRO:?devkitPro required}"
cc="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-gcc"
libnx="$DEVKITPRO/libnx"
nm="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-nm"
objdump="$DEVKITPRO/devkitA64/bin/aarch64-none-elf-objdump"
flags=(-std=gnu11 -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE -D__SWITCH__ -I"$libnx/include")
"$cc" --version > "$out/toolchain.txt"
printf 'symbol\tcompile\tlink\truntime\n' > "$out/abi.tsv"
probe() {
    local symbol=$1 header=$2
    local src="$out/$symbol.c" obj="$out/$symbol.o"
    printf '#include <%s>\n__typeof__(&%s) volatile ref = &%s;\nint main(void) { return ref == 0; }\n' "$header" "$symbol" "$symbol" > "$src"
    local c=no l=not_attempted
    if "$cc" "${flags[@]}" -Werror=implicit-function-declaration -c "$src" -o "$obj" > "$out/$symbol.compile.log" 2>&1; then
        c=yes
        l=no
        if "$cc" "${flags[@]}" -specs="$libnx/switch.specs" "$obj" -L"$libnx/lib" -lnx -o "$out/$symbol.elf" > "$out/$symbol.link.log" 2>&1; then
            l=yes
            "$nm" -n "$out/$symbol.elf" > "$out/$symbol.symbols.txt"
            if [[ "$symbol" =~ ^(fork|execve|kill|getpid|dup2)$ ]]; then
                "$objdump" -d --disassemble="$symbol" "$out/$symbol.elf" > "$out/$symbol.disassembly.txt"
                "$objdump" -d --disassemble="_$symbol" "$out/$symbol.elf" >> "$out/$symbol.disassembly.txt"
            fi
        fi
    fi
    printf '%s\t%s\t%s\tnot_tested\n' "$symbol" "$c" "$l" >> "$out/abi.tsv"
}
while read -r symbol header; do
    probe "$symbol" "$header"
done <<'EOF'
getpid unistd.h
getppid unistd.h
fork unistd.h
vfork unistd.h
execve unistd.h
waitpid sys/wait.h
wait3 sys/wait.h
pipe unistd.h
dup2 unistd.h
fcntl fcntl.h
open fcntl.h
read unistd.h
write unistd.h
close unistd.h
getcwd unistd.h
chdir unistd.h
stat sys/stat.h
opendir dirent.h
readdir dirent.h
poll poll.h
ppoll poll.h
clock_gettime time.h
sigaction signal.h
sigprocmask signal.h
kill signal.h
setsid unistd.h
setpgid unistd.h
tcgetpgrp unistd.h
tcsetpgrp unistd.h
tcsetsid termios.h
isatty unistd.h
fwopen stdio.h
funopen stdio.h
fopencookie stdio.h
fdopen stdio.h
fileno stdio.h
fflush stdio.h
fmemopen stdio.h
socket sys/socket.h
socketpair sys/socket.h
select sys/select.h
sigemptyset signal.h
sigaddset signal.h
sigsuspend signal.h
raise signal.h
getenv stdlib.h
setenv stdlib.h
unsetenv stdlib.h
setmode unistd.h
getmode unistd.h
strlcpy string.h
eaccess unistd.h
access unistd.h
faccessat unistd.h
getgid unistd.h
getegid unistd.h
getgroups unistd.h
umask sys/stat.h
chmod sys/stat.h
fchmod sys/stat.h
fstat sys/stat.h
dup unistd.h
nanosleep time.h
sigfillset signal.h
sigdelset signal.h
sigismember signal.h
pthread_sigmask pthread.h
getuid unistd.h
geteuid unistd.h
getpwnam pwd.h
getrlimit sys/resource.h
getrusage sys/resource.h
svcGetProcessId switch.h
EOF
cat "$out/abi.tsv"
# SDK sanity gate, not a runtime capability claim.
grep -q $'^svcGetProcessId\tyes\tyes\tnot_tested$' "$out/abi.tsv"

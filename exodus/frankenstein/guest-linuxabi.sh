#!/bin/sh
# Disposable FreeBSD guest only. No external Linux binaries or credentials.
set -eu
export PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin
exec >/dev/console 2>&1
finish() {
    rc=$?
    trap - EXIT
    echo "EXODUS_GUEST_RESULT=$rc"
    /sbin/shutdown -p now
    exit "$rc"
}
trap finish EXIT
test "$(uname -s)" = FreeBSD
test "$(uname -p)" = aarch64
uname -a
sysctl hw.model
service sshd onestop || :
sysrc sshd_enable=NO
work=$(mktemp -d /var/tmp/tumor-linux-abi.XXXXXX)
mkdir "$work/seed"
seed=
for d in /dev/msdosfs/[cC][iI][dD][aA][tT][aA]; do
    [ -e "$d" ] || continue
    seed=$d
    break
done
test -n "$seed"
mount -t msdosfs -o ro "$seed" "$work/seed"
for f in linux-armv8 linux-lse; do
    cp "$work/seed/$f" "$work/$f"
    chmod 755 "$work/$f"
done
umount "$work/seed"
chmod 755 "$work"
service linux onestart
kldstat
sysctl compat.linux.osrelease
file "$work/linux-armv8" "$work/linux-lse"
brandelf -t Linux "$work/linux-armv8" "$work/linux-lse"
pw useradd -n exodusref -m -s /bin/sh -w no
out=$(su -m exodusref -c "$work/linux-armv8")
test "$out" = TUMOR_LINUX_ABI_OK
echo EXODUS_LINUX_ARM64_SYSCALL_PASS
# FreeBSD su returns WEXITSTATUS even for a signalled child (releng/15.1
# usr.bin/su/su.c). Use a native waitpid supervisor instead of interpreting
# su's status as the Linux process status. Never assume LSE is supported.
cat > "$work/wait-lse.c" <<'C'
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
    struct rlimit no_core = {0, 0};
    if (argc != 2 || setrlimit(RLIMIT_CORE, &no_core) != 0) return 2;
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 2; }
    if (pid == 0) {
        execl(argv[1], argv[1], (char *)0);
        perror("execl");
        _exit(127);
    }
    int status;
    pid_t got;
    do { got = waitpid(pid, &status, 0); } while (got < 0 && errno == EINTR);
    if (got != pid) { perror("waitpid"); return 2; }
    if (WIFSIGNALED(status)) {
        printf("EXODUS_LSE_SIGNAL=%d\n", WTERMSIG(status));
        return WTERMSIG(status) == SIGILL ? 0 : 1;
    }
    if (WIFEXITED(status))
        printf("EXODUS_LSE_EXIT=%d\n", WEXITSTATUS(status));
    return 1;
}
C
cc -std=c11 -Wall -Wextra -Werror -O2 "$work/wait-lse.c" -o "$work/wait-lse"
su -m exodusref -c "exec $work/wait-lse $work/linux-lse"
echo EXODUS_LINUX_ABI_REFERENCE_PASS

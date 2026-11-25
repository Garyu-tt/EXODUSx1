#!/bin/sh
# QEMU-only fixture: original FreeBSD init -> OpenSSH loopback key exchange.
# No inbound network, no login or production host keys.
set -eu
export PATH=/rescue:/bin:/sbin:/usr/bin:/usr/sbin
umask 077
# FreeBSD mounts the RAM root read-only during initial boot. Only this
# disposable guest filesystem is updated; no host or physical disk is used.
/rescue/mount -uw /
# Prove actual write access before generating password DBs or host keys.
: > /tmp/tumor-root-write-test
printf '%s\n' 'TUMOR_BAO_REF: FREEBSD_RAMROOT_WRITABLE' > /dev/console
/rescue/ifconfig lo0 inet 127.0.0.1 up
/usr/sbin/pwd_mkdb -p /etc/master.passwd
/usr/bin/ssh-keygen -q -t ed25519 -N '' -f /etc/ssh/ssh_host_ed25519_key
/usr/sbin/sshd -t -f /etc/ssh/tumor-test.conf
/usr/sbin/sshd -f /etc/ssh/tumor-test.conf
read pid < /var/run/tumor-sshd.pid
case "$pid" in ''|*[!0-9]*) exit 1;; esac
test "$pid" -gt 1
kill -0 "$pid"
read expected_type expected_key comment < /etc/ssh/ssh_host_ed25519_key.pub
test "$expected_type" = ssh-ed25519
test -n "$expected_key"
probe() {
    /usr/bin/ssh-keyscan -T 5 -p 2222 -t ed25519 127.0.0.1 > /tmp/tumor-scan
    read host keytype key < /tmp/tumor-scan
    test "$keytype" = "$expected_type"
    test "$key" = "$expected_key"
    kill -0 "$pid"
}
probe
/rescue/sleep 3
probe
printf '%s\n' 'TUMOR_BAO_REF: FREEBSD_OPENSSH_LOOPBACK_KEX' > /dev/console
# Leave the loopback-only daemon running after rc returns; QEMU is bounded.
exit 0

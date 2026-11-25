#!/bin/sh
# QEMU-only fixture: original FreeBSD init -> OpenSSH loopback key exchange.
# QEMU-only authenticated command probe, no NIC or production keys.
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
/usr/bin/ssh-keygen -q -t ed25519 -N '' -f /tmp/tumor-client
# Only a temporary guest-local key authorizes this nonprivileged test account.
/rescue/cat /tmp/tumor-client.pub > /etc/ssh/tumor-authorized-keys
/rescue/chmod 0644 /etc/ssh/tumor-authorized-keys
/usr/sbin/sshd -t -f /etc/ssh/tumor-test.conf
# Run the actual daemon without daemonizing: use its child PID and retain
# stderr on the console rather than racing an asynchronously written PID file.
# All listeners remain restricted to the configured guest loopback.
/usr/sbin/sshd -D -e -o LogLevel=DEBUG2 -f /etc/ssh/tumor-test.conf </dev/null &
pid=$!
test "$pid" -gt 1
read expected_type expected_key comment < /etc/ssh/ssh_host_ed25519_key.pub
test "$expected_type" = ssh-ed25519
test -n "$expected_key"
probe() {
    scan_rc=0
    /usr/bin/ssh-keyscan -4 -v -T 15 -p 2222 -t ed25519 127.0.0.1 > /tmp/tumor-scan || scan_rc=$?
    if test "$scan_rc" -ne 0; then
        printf 'R29 keyscan exit=%s\n' "$scan_rc" >&2
        return 1
    fi
    # ssh-keyscan may emit protocol banners as comment lines before the
    # actual known_hosts record. Do not treat a banner as a key or accept
    # a successful scan without the exact generated ED25519 public key.
    records=0
    while read -r host keytype key extra; do
        case "$host" in
            ''|\#*) continue ;;
        esac
        if test "$host" != '[127.0.0.1]:2222' ||
           test "$keytype" != "$expected_type" ||
           test "$key" != "$expected_key" || test -n "$extra"; then
            printf 'R29 keyscan returned unexpected host/key record: host=%s type=%s\n' "$host" "$keytype" >&2
            return 1
        fi
        records=$((records + 1))
    done < /tmp/tumor-scan
    if test "$records" -ne 1; then
        printf 'R29 keyscan expected one verified record, got %s\n' "$records" >&2
        return 1
    fi
    if ! kill -0 "$pid"; then
        printf '%s\n' 'R29 listener PID no longer alive' >&2
        return 1
    fi
    printf '%s\n' 'R29 verified generated ED25519 public key' >&2
}
# Allow bounded listener startup, but reject a dead daemon immediately.
ready=no
for attempt in 1 2 3 4; do
    printf 'R29 listener attempt=%s\n' "$attempt" >&2
    kill -0 "$pid"
    if probe; then ready=yes; break; fi
    /rescue/sleep 1
done
test "$ready" = yes
printf '%s\n' 'TUMOR_BAO_REF: FREEBSD_OPENSSH_LISTENER_READY' > /dev/console
/rescue/sleep 3
probe
printf '%s\n' 'TUMOR_BAO_REF: FREEBSD_OPENSSH_LOOPBACK_KEX' > /dev/console
# R30: pin the generated host key independently of ssh-keyscan output.
printf '[127.0.0.1]:2222 %s %s\n' "$expected_type" "$expected_key" > /tmp/tumor-known-hosts
client() {
    /usr/bin/ssh -F /dev/null -4 -T -p 2222 -i /tmp/tumor-client \
        -o BatchMode=yes -o IdentitiesOnly=yes -o IdentityAgent=none \
        -o StrictHostKeyChecking=yes -o UserKnownHostsFile=/tmp/tumor-known-hosts \
        -o GlobalKnownHostsFile=/dev/null -o ConnectTimeout=20 \
        -o PreferredAuthentications=publickey -o NumberOfPasswordPrompts=0 \
        tumor-probe@127.0.0.1 "$@"
}
result=$(client 'test "$(/rescue/id -u)" = 60001 && printf "%s" TUMOR_AUTH_COMMAND_OK')
test "$result" = TUMOR_AUTH_COMMAND_OK
printf '%s\n' 'TUMOR_BAO_REF: FREEBSD_OPENSSH_AUTH_COMMAND' > /dev/console
remote_rc=0
client 'exit 23' > /tmp/tumor-exit-output || remote_rc=$?
test "$remote_rc" -eq 23
test ! -s /tmp/tumor-exit-output
printf '%s\n' 'TUMOR_BAO_REF: FREEBSD_OPENSSH_REMOTE_EXIT_23' > /dev/console
# Leave the loopback-only daemon running after rc returns; QEMU is bounded.
exit 0

#!/bin/sh
# Reference FreeBSD only. No installation onto Switch or production host.
set -eu
exec >/dev/console 2>&1
work=
finish() {
    rc=$?
    trap - EXIT
    if [ -n "$work" ] && [ -f "$work/sshd.pid" ]; then
        kill "$(cat "$work/sshd.pid")" 2>/dev/null || :
    fi
    echo "EXODUS_GUEST_RESULT=$rc"
    /sbin/shutdown -p now
    exit "$rc"
}
trap finish EXIT
echo EXODUS_SSH_GUEST_BEGIN
test "$(uname -s)" = FreeBSD
test "$(uname -p)" = aarch64
uname -a
freebsd-version -ku
service sshd onestop || :
sysrc sshd_enable=NO
work=$(mktemp -d /var/tmp/exodus-ssh.XXXXXX)
chmod 700 "$work"
pw useradd -n exodusref -m -s /bin/sh -w no
home=/home/exodusref
test -d "$home"
install -d -m 700 -o exodusref -g exodusref "$home/.ssh" "$home/.config" "$home/.config/exodus"
ssh-keygen -q -t ed25519 -N '' -f "$work/host"
ssh-keygen -q -t ed25519 -N '' -f "$work/client"
ssh-keygen -q -t ed25519 -N '' -f "$work/unknown"
install -m 600 -o exodusref -g exodusref "$work/client.pub" "$home/.ssh/authorized_keys"
# Reference implementation of a narrow backend for desired session selection.
# It persists intent only, never claims the compositor or desktop has started.
cat > "$work/session-select" <<'SCRIPT'
#!/bin/sh
set -eu
umask 077
state="$HOME/.config/exodus"
case "${1:-status}" in
    status)
        if [ -f "$state/desired-mode" ]; then
            read -r mode < "$state/desired-mode"
            case "$mode" in game|desktop) ;; *) exit 1 ;; esac
        else
            mode=game
        fi
        printf 'desired=%s\nactive=unknown\n' "$mode"
        ;;
    gamescope|plasma|plasma-wayland)
        case "$1" in gamescope) mode=game ;; *) mode=desktop ;; esac
        temp=$(mktemp "$state/.desired-mode.XXXXXX")
        trap 'rm -f "$temp"' EXIT
        printf '%s\n' "$mode" > "$temp"
        mv -f "$temp" "$state/desired-mode"
        printf 'requested=%s\n' "$mode"
        ;;
    *) echo 'unsupported session' >&2; exit 2 ;;
esac
SCRIPT
install -m 755 "$work/session-select" /usr/local/bin/exodus-session-select
cat > "$work/sshd_config" <<EOF
Port 22222
ListenAddress 127.0.0.1
HostKey $work/host
PidFile $work/sshd.pid
AuthorizedKeysFile .ssh/authorized_keys
AllowUsers exodusref
PermitRootLogin no
PasswordAuthentication no
KbdInteractiveAuthentication no
PubkeyAuthentication yes
AuthenticationMethods publickey
UsePAM no
StrictModes yes
AllowTcpForwarding no
AllowAgentForwarding no
X11Forwarding no
PermitTunnel no
PermitUserEnvironment no
MaxAuthTries 2
MaxSessions 2
LogLevel VERBOSE
EOF
/usr/sbin/sshd -t -f "$work/sshd_config"
/usr/sbin/sshd -f "$work/sshd_config" -E "$work/sshd.log"
pid=$(cat "$work/sshd.pid")
kill -0 "$pid"
printf '[127.0.0.1]:22222 %s\n' "$(cut -d ' ' -f 1,2 "$work/host.pub")" > "$work/known_hosts"
connect() {
    identity=$1
    shift
    ssh -F /dev/null -p 22222 -i "$identity" -o IdentitiesOnly=yes \
        -o BatchMode=yes -o StrictHostKeyChecking=yes \
        -o UserKnownHostsFile="$work/known_hosts" -o GlobalKnownHostsFile=/dev/null \
        -o ConnectTimeout=5 exodusref@127.0.0.1 "$@"
}
test "$(connect "$work/client" id -un)" = exodusref
if connect "$work/unknown" id -un; then
    echo 'ERROR unknown key accepted'
    exit 1
fi
echo EXODUS_SSH_AUTH_PASS
connect "$work/client" exodus-session-select status | grep -Fx 'desired=game'
connect "$work/client" exodus-session-select plasma | grep -Fx 'requested=desktop'
connect "$work/client" exodus-session-select status | grep -Fx 'desired=desktop'
if connect "$work/client" exodus-session-select invalid; then exit 1; fi
connect "$work/client" exodus-session-select status | grep -Fx 'desired=desktop'
connect "$work/client" exodus-session-select gamescope | grep -Fx 'requested=game'
connect "$work/client" exodus-session-select status | grep -Fx 'desired=game'
test "$pid" = "$(cat "$work/sshd.pid")"
kill -0 "$pid"
echo EXODUS_SESSION_INTENT_WITHOUT_REBOOT_PASS
# Revocation is checked for a new connection, not existing authenticated sessions.
: > "$home/.ssh/authorized_keys"
if connect "$work/client" id -un; then
    echo 'ERROR revoked key accepted'
    exit 1
fi
echo EXODUS_SSH_REVOCATION_PASS
cat "$work/sshd.log"
echo EXODUS_SSH_REFERENCE_PASS

#!/bin/sh
# Reference FreeBSD only. No installation onto Switch or production host.
set -eu
# nuageinit inherits the base-system rc PATH, not a login shell PATH.
export PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin
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
# Build and connect the actual native daemon, not an independent state file.
src=$(mktemp -d /var/tmp/tumor-resident.XXXXXX)
chmod 755 "$src"
mkdir "$src/seed"
seed=
for d in /dev/msdosfs/[cC][iI][dD][aA][tT][aA]; do
    [ -e "$d" ] || continue
    seed=$d
    break
done
test -n "$seed"
mount -t msdosfs -o ro "$seed" "$src/seed"
for f in sessiond.c sessiond-dbus.inc; do cp "$src/seed/$f" "$src/$f"; done
umount "$src/seed"
export ASSUME_ALWAYS_YES=yes
pkg bootstrap -y
pkg update -f
pkg install -y glib dbus pkgconf
cc -std=c11 -Wall -Wextra -Werror -O2 -DEXODUS_DBUS "$src/sessiond.c" $(pkgconf --cflags --libs gio-2.0) -o "$src/sessiond"
install -m 755 "$src/sessiond" /usr/local/bin/exodus-sessiond-dbus
state="$home/.config/exodus"
printf 'generation=1\nmode=game\n' > "$state/config"
chown exodusref:exodusref "$state/config"
chmod 600 "$state/config"
# Fixed owner-only bus socket. It is independent of SSH connection lifetimes.
cat > "$src/start" <<'SCRIPT'
#!/bin/sh
set -eu
umask 077
state="$HOME/.config/exodus"
export DBUS_SESSION_BUS_ADDRESS="unix:path=$state/bus"
dbus-daemon --session --address="$DBUS_SESSION_BUS_ADDRESS" --fork --print-pid > "$state/bus.pid"
nohup /usr/local/bin/exodus-sessiond-dbus "$state/config" > "$state/daemon.log" 2>&1 </dev/null &
echo "$!" > "$state/daemon.pid"
SCRIPT
# su -m preserves root HOME on some systems; set the intended HOME explicitly.
su -m exodusref -c "HOME=$home /bin/sh $src/start"
cat > "$src/session-select" <<'SCRIPT'
#!/bin/sh
set -eu
export PATH=/sbin:/bin:/usr/sbin:/usr/bin:/usr/local/sbin:/usr/local/bin
export DBUS_SESSION_BUS_ADDRESS="unix:path=$HOME/.config/exodus/bus"
name=com.steampowered.SteamOSManager1
path=/com/steampowered/SteamOSManager1
iface=$name.SessionManagement1
call() { gdbus call --session --timeout 5 --dest "$name" --object-path "$path" --method "$@"; }
case "${1:-status}" in
    status)
        case "$(call org.freedesktop.DBus.Properties.Get "$iface" DefaultLoginMode)" in
            *"'game'"*) printf 'desired=game\nactive=unknown\n' ;;
            *"'desktop'"*) printf 'desired=desktop\nactive=unknown\n' ;;
            *) exit 1 ;;
        esac ;;
    gamescope|plasma|plasma-wayland)
        case "$1" in gamescope) mode=game ;; *) mode=desktop ;; esac
        call org.freedesktop.DBus.Properties.Set "$iface" DefaultLoginMode "<'$mode'>" >/dev/null
        printf 'requested=%s\n' "$mode" ;;
    *) exit 2 ;;
esac
SCRIPT
install -m 755 "$src/session-select" /usr/local/bin/exodus-session-select
su -m exodusref -c "HOME=$home DBUS_SESSION_BUS_ADDRESS=unix:path=$state/bus /usr/local/bin/gdbus wait --session --timeout 15 com.steampowered.SteamOSManager1"
daemon_pid=$(cat "$state/daemon.pid")
kill -0 "$daemon_pid"
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
test "$daemon_pid" = "$(cat "$state/daemon.pid")"
kill -0 "$daemon_pid"
grep -Fx 'generation=3' "$state/config"
grep -Fx 'mode=game' "$state/config"
grep -F "reloaded generation=2 desired=desktop active=unknown pid=$daemon_pid" "$state/daemon.log"
grep -F "reloaded generation=3 desired=game active=unknown pid=$daemon_pid" "$state/daemon.log"
echo EXODUS_SESSION_INTENT_WITHOUT_REBOOT_PASS
# Revocation is checked for a new connection, not existing authenticated sessions.
: > "$home/.ssh/authorized_keys"
if connect "$work/client" id -un; then
    echo 'ERROR revoked key accepted'
    exit 1
fi
echo EXODUS_SSH_REVOCATION_PASS
cat "$work/sshd.log"
kill -TERM "$daemon_pid"
kill -TERM "$(cat "$state/bus.pid")"
echo EXODUS_RESIDENT_SSH_DBUS_PASS

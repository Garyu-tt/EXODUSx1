#!/bin/sh
set -eu
binary=$1
tmp=$(mktemp -d)
pid=
cleanup() {
    if [ -n "$pid" ]; then kill "$pid" 2>/dev/null || :; wait "$pid" 2>/dev/null || :; fi
    rm -rf "$tmp"
}
trap cleanup EXIT
umask 077
conf="$tmp/config"
printf 'generation=1\nmode=game\n' > "$conf"
"$binary" "$conf" > "$tmp/log" 2>&1 &
pid=$!
bus=com.steampowered.SteamOSManager1
iface=$bus.SessionManagement1
path=/com/steampowered/SteamOSManager1
gdbus wait --session --timeout 10 "$bus"
call() { gdbus call --session --dest "$bus" --object-path "$path" --method "$@"; }
get_mode() { call org.freedesktop.DBus.Properties.Get "$iface" DefaultLoginMode; }
get_mode | grep -F "'game'"
call org.freedesktop.DBus.Properties.Set "$iface" DefaultLoginMode "<'desktop'>"
get_mode | grep -F "'desktop'"
grep -Fx 'generation=2' "$conf"
grep -Fx 'mode=desktop' "$conf"
if call org.freedesktop.DBus.Properties.Set "$iface" DefaultLoginMode "<'invalid'>"; then exit 1; fi
get_mode | grep -F "'desktop'"
printf 'generation=3\nmode=game\n' > "$tmp/new"
mv "$tmp/new" "$conf"
call "$bus.Manager2.ReloadConfig"
get_mode | grep -F "'game'"
printf 'generation=4\nmode=invalid\n' > "$tmp/new"
mv "$tmp/new" "$conf"
if call "$bus.Manager2.ReloadConfig"; then exit 1; fi
get_mode | grep -F "'game'"
if call "$iface.SwitchToGameMode"; then exit 1; fi
if call "$iface.SwitchToDesktopMode"; then exit 1; fi
call "$iface.ValidDesktopSessions" | grep -F '[]'
kill -0 "$pid"
kill -TERM "$pid"
wait "$pid"
pid=
cat "$tmp/log"
grep -E '^reloaded generation=2 desired=desktop active=unknown pid=[0-9]+$' "$tmp/log"
grep -E '^reloaded generation=3 desired=game active=unknown pid=[0-9]+$' "$tmp/log"
echo EXODUS_DBUS_REFERENCE_PASS

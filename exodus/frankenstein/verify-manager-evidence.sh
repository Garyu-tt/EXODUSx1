#!/bin/sh
# Validate manager compile evidence on the host; an audit alone is not a port.
set -eu
if [ "$#" -ne 2 ]; then
    echo 'Usage: verify-manager-evidence.sh SERIAL_LOG NORMALIZED_LOG' >&2
    exit 2
fi
tr -d '\r' < "$1" > "$2"
grep -Fx EXODUS_MANAGER_PROTOCOL_BUILD_PASS "$2"
if ! awk '
    /^EXODUS_MANAGER_DAEMON_CHECK_RESULT=/ {
        count++
        if ($0 != "EXODUS_MANAGER_DAEMON_CHECK_RESULT=0") invalid=1
    }
    END { exit !(count == 1 && !invalid) }
' "$2"; then
    echo 'Upstream manager daemon compile gate not passed (expected exactly one result=0)' >&2
    exit 1
fi
echo EXODUS_MANAGER_DAEMON_COMPILE_PASS

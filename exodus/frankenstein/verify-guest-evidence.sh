#!/bin/sh
# Require a complete guest success marker and one exact zero exit record.
set -eu
if [ "$#" -ne 4 ]; then
    echo 'Usage: verify-guest-evidence.sh SERIAL NORMALIZED SUCCESS_MARKER RESULT_KEY' >&2
    exit 2
fi
# Normalize console CRLF without removing embedded carriage returns.
awk '{ sub(/\r$/, ""); print }' "$1" > "$2"
if ! awk -v gate="$3" -v key="$4=" '
    $0 == gate { passed=1 }
    index($0, key) == 1 {
        count++
        if ($0 != key "0") invalid=1
        if (!passed) invalid=1
    }
    END { exit !(passed && count == 1 && !invalid) }
' "$2"; then
    echo "Guest evidence gate not passed: $3 / $4" >&2
    exit 1
fi
grep -Fx "$3" "$2"
grep -Fx "$4=0" "$2"

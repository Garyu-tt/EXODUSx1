#!/bin/sh
# Isolated QEMU root-image fixture, executed by the original FreeBSD init.
# Not the stock /etc/rc, not a production boot script.
set -eu
test "$(/rescue/sysctl -n kern.ostype)" = FreeBSD
set -- $(/rescue/ps -p 1 -o pid=)
test "$#" -eq 1
test "$1" = 1
test -c /dev/console
printf '%s\n' 'TUMOR_BAO_REF: FREEBSD_STOCK_INIT_RC' > /dev/console
exit 0

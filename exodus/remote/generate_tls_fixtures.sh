#!/bin/sh
set -eu
out=$1
d=$(mktemp -d)
trap 'rm -rf "$d"' EXIT HUP INT TERM
cd "$d"
umask 077
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -sha256 -days 30 -subj '/CN=EXODUS TEST ONLY CA' -addext 'basicConstraints=critical,CA:TRUE,pathlen:0' -addext 'keyUsage=critical,keyCertSign,cRLSign' -keyout ca.key -out ca.pem
for role in server client; do
  openssl req -new -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -subj "/CN=EXODUS TEST ONLY $role" -keyout "$role.key" -out "$role.csr"
  printf 'basicConstraints=critical,CA:FALSE\nkeyUsage=critical,digitalSignature\n' > "$role.ext"
  if [ "$role" = server ]; then
    printf 'extendedKeyUsage=serverAuth\nsubjectAltName=DNS:exodus-smoke.invalid\n' >> "$role.ext"
  else
    printf 'extendedKeyUsage=clientAuth\n' >> "$role.ext"
  fi
  openssl x509 -req -in "$role.csr" -CA ca.pem -CAkey ca.key -CAcreateserial -days 30 -sha256 -extfile "$role.ext" -out "$role.pem"
done
python3 - "$out" <<'PY'
import sys
from pathlib import Path
with open(sys.argv[1], 'w') as f:
    f.write('/* PUBLIC TEST ONLY credentials; never trust in a deployed server. */\n')
    for name in ('ca.pem','server.pem','server.key','client.pem','client.key'):
        data=Path(name).read_bytes()+b'\0'
        f.write('static const unsigned char fixture_'+name.replace('.','_')+'[] = {'+','.join(map(str,data))+'};\n')
PY

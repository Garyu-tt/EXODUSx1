#!/bin/sh
set -eu
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
d=$(mktemp -d); trap 'rm -rf "$d"' EXIT HUP INT TERM
cd "$d"
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -days 1 -subj /CN=EXR-test-CA -keyout ca.key -out ca.pem 2>/dev/null
for name in server client; do
 openssl req -new -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -subj /CN=localhost -keyout "$name.key" -out "$name.csr" 2>/dev/null
 if [ "$name" = server ]; then printf 'subjectAltName=DNS:localhost\nextendedKeyUsage=serverAuth\n' > ext; else printf 'extendedKeyUsage=clientAuth\n' > ext; fi
 openssl x509 -req -in "$name.csr" -CA ca.pem -CAkey ca.key -CAcreateserial -days 1 -extfile ext -out "$name.pem" 2>/dev/null
done
openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:P-256 -nodes -days 1 -subj /CN=rogue -keyout rogue.key -out rogue.pem 2>/dev/null
cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -no-pie "$src/test_remote_session.c" -o test $(pkg-config --cflags --libs openssl)
./test

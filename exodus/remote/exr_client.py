"""EXR/1 read-only client; TLS 1.3 with explicit CA and server identity."""
import argparse
import socket
import ssl
import struct
import sys
import time

HEADER = struct.Struct('!2sBBHH')
OPERATIONS = ('capabilities.list', 'system.status', 'battery.read', 'storage.read')


def remaining(deadline):
    left = deadline - time.monotonic()
    if left <= 0:
        raise TimeoutError('request deadline expired')
    return left


def exact(sock, count, deadline):
    data = bytearray()
    while len(data) < count:
        sock.settimeout(remaining(deadline))
        chunk = sock.recv(count - len(data))
        if not chunk:
            raise EOFError('truncated EXR response')
        data.extend(chunk)
    return bytes(data)


def exchange(sock, operation, deadline):
    if operation not in OPERATIONS:
        raise ValueError('unsupported read-only operation')
    body = operation.encode('ascii')
    sock.settimeout(remaining(deadline))
    sock.sendall(HEADER.pack(b'EX', 1, 0, len(body), 0) + body)
    magic, version, status, length, reserved = HEADER.unpack(exact(sock, HEADER.size, deadline))
    if (magic, version, reserved) != (b'EX', 1, 0) or status > 4 or length > 511 or (status and length):
        raise ValueError('invalid EXR response header')
    return status, exact(sock, length, deadline)


def request(port, identity, ca, cert, key, operation, timeout=8.0):
    if operation not in OPERATIONS or not 1 <= port <= 65535 or not 0 < timeout <= 30:
        raise ValueError('invalid operation, port or timeout')
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
    context.minimum_version = context.maximum_version = ssl.TLSVersion.TLSv1_3
    context.load_verify_locations(cafile=ca)
    context.load_cert_chain(cert, key)
    # The current server is deliberately loopback-only. No LAN binding here.
    deadline = time.monotonic() + timeout
    with socket.create_connection(('127.0.0.1', port), timeout=remaining(deadline)) as raw:
        raw.settimeout(remaining(deadline))
        with context.wrap_socket(raw, server_hostname=identity) as tls:
            return exchange(tls, operation, deadline)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port', type=int, required=True)
    p.add_argument('--identity', default='localhost')
    for name in ('ca', 'cert', 'key'):
        p.add_argument('--' + name, required=True)
    p.add_argument('operation', choices=OPERATIONS)
    a = p.parse_args()
    try:
        status, body = request(a.port, a.identity, a.ca, a.cert, a.key, a.operation)
        if status:
            print('EXR status=' + str(status), file=sys.stderr)
            return 1
        # Escape terminal controls; never interpret untrusted provider output.
        print(''.join(chr(b) if b == 10 or 32 <= b <= 126 else '\\x%02x' % b for b in body), end='')
        return 0
    except (OSError, EOFError, ValueError) as exc:
        print('EXR request failed: ' + str(exc), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())

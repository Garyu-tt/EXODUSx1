"""Observe a QEMU-only synthetic peer across two authenticated BSD commands."""
import json
import re
import socket
import sys
import time
import unittest
from pathlib import Path

SENTINEL = 0x544d525045455231
AUTH = b'TUMOR_BAO_REF: FREEBSD_OPENSSH_AUTH_COMMAND'
EXIT = b'TUMOR_BAO_REF: FREEBSD_OPENSSH_REMOTE_EXIT_23'


def counter(registers):
    # Match the sentinel and counter in the same CPU block, never mix CPUs.
    candidates = []
    for block in re.split(r'(?m)^\s*CPU#\d+\s*$', registers):
        values = {key: int(value, 16) for key, value in
                  re.findall(r'\b(PC|X09|X10)=([0-9a-fA-F]+)\b', block)}
        if (values.get('X10') == SENTINEL and
                0x60000000 <= values.get('PC', 0) < 0x60001000 and
                re.search(r'\bEL1[ht]\b', block)):
            candidates.append(values['X09'])
    if len(candidates) != 1:
        raise ValueError('expected one resident EL1 peer register block')
    return candidates[0]


def wait_marker(log, marker, deadline):
    while time.monotonic() < deadline:
        if log.exists() and marker in log.read_bytes().replace(b'\r', b'').splitlines():
            return
        time.sleep(0.1)
    raise TimeoutError('missing guest milestone: ' + marker.decode())


def observe(endpoint, log):
    deadline = time.monotonic() + 270
    while not endpoint.exists() and time.monotonic() < deadline:
        time.sleep(0.1)
    with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as conn:
        conn.settimeout(5)
        conn.connect(str(endpoint))
        with conn.makefile('rb') as reader:
            def receive():
                raw = reader.readline(262145)
                if not raw or len(raw) > 262144:
                    raise ValueError('invalid QMP frame')
                return json.loads(raw)
            if 'QMP' not in receive():
                raise ValueError('missing QMP greeting')
            def command(name, arguments=None):
                message = {'execute': name, 'id': name}
                if arguments is not None:
                    message['arguments'] = arguments
                conn.sendall(json.dumps(message).encode() + b'\n')
                for _ in range(32):
                    reply = receive()
                    if reply.get('id') == name:
                        if 'return' not in reply:
                            raise ValueError('QMP command failed: ' + str(reply))
                        return reply['return']
                raise ValueError('QMP response missing')
            command('qmp_capabilities')
            def sample():
                return counter(command('human-monitor-command',
                                       {'command-line': 'info registers -a'}))
            wait_marker(log, AUTH, deadline)
            if EXIT in log.read_bytes().replace(b'\r', b'').splitlines():
                raise ValueError('missed interval between authenticated commands')
            first = sample()
            wait_marker(log, EXIT, deadline)
            second = sample()
            if second <= first:
                raise ValueError('peer did not progress across BSD commands')
            print(f'EXODUS_BAO_RESIDENT_PEER_PASS first={first} second={second}')


class CounterTest(unittest.TestCase):
    def test_one_peer(self):
        peer = 'CPU#0\n PC=0000000060000040 X09=000000000000000a X10=544d525045455231 PSTATE=00000005 EL1h\n'
        other = 'CPU#3\n PC=0000000060000040 X09=000000000000000b X10=0000000000000000 PSTATE=00000005 EL1h\n'
        self.assertEqual(counter(peer + other), 10)
        for bad in (other, peer.replace('EL1h', 'EL2h'), peer + peer):
            with self.assertRaises(ValueError):
                counter(bad)


if __name__ == '__main__':
    if sys.argv[1:] == ['--selftest']:
        unittest.main(argv=[sys.argv[0]])
    else:
        if len(sys.argv) != 3:
            raise SystemExit('usage: verify_bao_peer.py qmp.sock serial.log')
        observe(Path(sys.argv[1]), Path(sys.argv[2]))

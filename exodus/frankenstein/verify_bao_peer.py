"""Observe a QEMU-only peer across two authenticated BSD commands."""
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


def counter(registers, headless=False):
    sentinel = 0x544d525045455232 if headless else SENTINEL
    start = 0x400000 if headless else 0x60000000
    privilege = r'\bEL0t\b' if headless else r'\bEL1[ht]\b'
    # Match the sentinel and counter in the same CPU block, never mix CPUs.
    candidates = []
    for block in re.split(r'(?m)^\s*CPU#\d+\s*$', registers):
        values = {key: int(value, 16) for key, value in
                  re.findall(r'\b(PC|X09|X10)=([0-9a-fA-F]+)\b', block)}
        if (values.get('X10') == sentinel and 'X09' in values and
                start <= values.get('PC', 0) < start + 0x1000 and
                re.search(privilege, block)):
            candidates.append(values['X09'])
    if len(candidates) != 1:
        raise ValueError('expected one resident peer register block')
    return candidates[0]


def wait_marker(log, marker, deadline):
    while time.monotonic() < deadline:
        if log.exists() and marker in log.read_bytes().replace(b'\r', b'').splitlines():
            return
        time.sleep(0.1)
    raise TimeoutError('missing guest milestone: ' + marker.decode())


def observe(endpoint, log, headless=False):
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
                # Real PID 1 can be interrupted inside a kernel syscall.
                sample_deadline = min(deadline, time.monotonic() + 2)
                while True:
                    registers = command('human-monitor-command',
                                        {'command-line': 'info registers -a'})
                    try:
                        return counter(registers, headless)
                    except ValueError:
                        if not headless or time.monotonic() >= sample_deadline:
                            raise
                        time.sleep(0.05)
            wait_marker(log, AUTH, deadline)
            if EXIT in log.read_bytes().replace(b'\r', b'').splitlines():
                raise ValueError('missed interval between authenticated commands')
            first = sample()
            if EXIT in log.read_bytes().replace(b'\r', b'').splitlines():
                raise ValueError('first sample missed SSH interval')
            wait_marker(log, EXIT, deadline)
            second = sample()
            if second <= first:
                raise ValueError('peer did not progress across BSD commands')
            marker = 'EXODUS_BAO_DUAL_FREEBSD_PASS' if headless else 'EXODUS_BAO_RESIDENT_PEER_PASS'
            print(f'{marker} first={first} second={second}')


class CounterTest(unittest.TestCase):
    def test_headless_pid1(self):
        peer = 'CPU#0\n PC=0000000000400018 X09=000000000000002a X10=544d525045455232 PSTATE=00000000 EL0t\n'
        self.assertEqual(counter(peer, True), 42)
        for bad in (peer.replace('EL0t', 'EL1h'), peer.replace('00400018', '00600018'),
                    peer.replace('455232', '455231'), peer + peer):
            with self.assertRaises(ValueError):
                counter(bad, True)
        with self.assertRaises(ValueError):
            counter(peer)

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
        if len(sys.argv) not in (3, 4) or (len(sys.argv) == 4 and sys.argv[3] != '--headless-pid1'):
            raise SystemExit('usage: verify_bao_peer.py qmp.sock serial.log [--headless-pid1]')
        observe(Path(sys.argv[1]), Path(sys.argv[2]), len(sys.argv) == 4)

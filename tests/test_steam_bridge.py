import json
import os
from pathlib import Path
import socket
import tempfile
import threading
import unittest

from exodus.steam_bridge import BridgeError, exchange


@unittest.skipUnless(hasattr(os, 'getuid') and hasattr(socket, 'AF_UNIX'), 'POSIX sockets required')
class BridgeTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.path = Path(tmp.name).resolve() / 'bridge.sock'
        self.target = {'version': 1, 'backend': 'horizon', 'kind': 'title_id', 'id': '010000000000ABCD'}

    def peer(self, events, verb='waitforexitandrun', timeout=2):
        received = []
        failures = []
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as server:
            server.bind(str(self.path))
            self.path.chmod(0o600)
            server.listen(1)
            server.settimeout(2)

            def worker():
                try:
                    with server.accept()[0] as conn, conn.makefile('rb') as reader:
                        conn.settimeout(2)
                        req = json.loads(reader.readline(4096))
                        received.append(req)
                        for event in events:
                            message = dict(version=1, request_id=req['request_id'], **event)
                            conn.sendall(json.dumps(message).encode() + b'\n')
                except Exception as exc:
                    failures.append(exc)

            thread = threading.Thread(target=worker)
            thread.start()
            try:
                return exchange(self.path, 'exodus-horizon', verb, self.target, timeout)
            finally:
                thread.join(3)
                self.path.unlink()
                self.assertFalse(thread.is_alive())
                self.assertFalse(failures)
                self.assertEqual(received[0]['id'], '010000000000abcd')
                self.assertTrue(received[0]['requires_confirmation'])

    def test_exit_tracking(self):
        for verb in ('run', 'waitforexitandrun'):
            self.assertEqual(self.peer([{'state': 'accepted'}, {'state': 'running'},
                                        {'state': 'exited', 'exit_code': 7}], verb),
                             {'status': 'exited', 'exit_code': 7})

    def test_cancel_and_reject(self):
        self.assertEqual(self.peer([{'state': 'accepted'}, {'state': 'canceled'}])['exit_code'], 3)
        self.assertEqual(self.peer([{'state': 'rejected'}])['exit_code'], 1)

    def test_accepted_only_is_not_success(self):
        with self.assertRaises(BridgeError):
            self.peer([{'state': 'accepted'}])

    def test_illegal_transitions(self):
        for events in ([{'state': 'exited', 'exit_code': 0}],
                       [{'state': 'accepted'}, {'state': 'accepted'}],
                       [{'state': 'accepted'}, {'state': 'running'}, {'state': 'exited', 'exit_code': True}],
                       [{'state': 'accepted', 'extra': 'invalid'}]):
            with self.assertRaises(BridgeError):
                self.peer(events)

    def test_endpoint_permissions(self):
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as server:
            server.bind(str(self.path))
            self.path.chmod(0o666)
            with self.assertRaisesRegex(BridgeError, 'owner-only'):
                exchange(self.path, 'exodus-horizon', 'run', self.target)

    def test_missing_endpoint_and_bad_timeout(self):
        with self.assertRaises(OSError):
            exchange(self.path, 'exodus-horizon', 'run', self.target)
        with self.assertRaises(ValueError):
            exchange(self.path, 'exodus-horizon', 'run', self.target, float('nan'))


if __name__ == '__main__':
    unittest.main()

import os
from pathlib import Path
import socket
import tempfile
import threading
import unittest

from exodus.steam_bridge import BridgeError, exchange
from exodus.steam_bridge_listener import listener, serve_once


@unittest.skipUnless(hasattr(os, 'getuid') and hasattr(socket, 'AF_UNIX'), 'POSIX sockets required')
class ListenerTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name).resolve()
        self.path = self.root / 'bridge.sock'
        self.target = {'version': 1, 'backend': 'horizon', 'kind': 'nro', 'id': 'demo.nro'}

    def test_real_transport(self):
        errors = []
        with listener(self.path) as server:
            def worker():
                try:
                    serve_once(server, lambda plan: iter([('accepted', None), ('running', None), ('exited', 7)]))
                    serve_once(server)
                except Exception as exc:
                    errors.append(exc)
            thread = threading.Thread(target=worker)
            thread.start()
            try:
                self.assertEqual(exchange(self.path, 'exodus-horizon', 'run', self.target, 3)['exit_code'], 7)
                self.assertEqual(exchange(self.path, 'exodus-horizon', 'run', self.target, 3)['status'], 'rejected')
            finally:
                thread.join(12)
            self.assertFalse(thread.is_alive())
            self.assertFalse(errors)
        self.assertFalse(self.path.exists())

    def test_existing_endpoint_preserved(self):
        self.path.write_text('owner data')
        with self.assertRaises(BridgeError), listener(self.path):
            self.fail('must not replace endpoint')
        self.assertEqual(self.path.read_text(), 'owner data')

    def test_untrusted_directory(self):
        self.root.chmod(0o755)
        with self.assertRaises(BridgeError), listener(self.path):
            self.fail('must not bind in shared directory')
        self.assertFalse(self.path.exists())

    def test_replacement_not_removed(self):
        with listener(self.path):
            self.path.unlink()
            self.path.write_text('replacement')
        self.assertEqual(self.path.read_text(), 'replacement')


if __name__ == '__main__':
    unittest.main()

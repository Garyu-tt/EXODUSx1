import json
import socket
import threading
import unittest

from exodus.steam_bridge import BridgeError
from exodus.steam_bridge_server import serve_connection


@unittest.skipUnless(hasattr(socket, 'socketpair'), 'socketpair required')
class ServerTest(unittest.TestCase):
    def run_peer(self, adapter, **changes):
        request = {'version': 1, 'request_id': 'a' * 32, 'backend': 'horizon', 'kind': 'nro',
                   'id': 'tools/app.nro', 'verb': 'run', 'requires_confirmation': True}
        request.update(changes)
        client, server = socket.socketpair()
        failures = []
        def worker():
            with server:
                try:
                    serve_connection(server, adapter)
                except BridgeError as exc:
                    failures.append(str(exc))
        thread = threading.Thread(target=worker)
        thread.start()
        with client:
            client.settimeout(3)
            client.sendall(json.dumps(request).encode() + b'\n')
            with client.makefile('rb') as reader:
                events = [json.loads(line) for line in reader]
        thread.join(3)
        self.assertFalse(thread.is_alive())
        return events, failures

    def test_no_adapter(self):
        events, errors = self.run_peer(None)
        self.assertEqual([e['state'] for e in events], ['rejected'])
        self.assertFalse(errors)

    def test_observed_lifecycle(self):
        def adapter(plan):
            self.assertEqual(plan['argv'], ['--native-target', 'homebrew', 'tools/app.nro'])
            self.assertTrue(plan['requires_confirmation'])
            yield 'accepted', None
            yield 'running', None
            yield 'exited', 9
        events, errors = self.run_peer(adapter)
        self.assertEqual([e['state'] for e in events], ['accepted', 'running', 'exited'])
        self.assertEqual(events[-1]['exit_code'], 9)
        self.assertFalse(errors)

    def test_reject_before_adapter(self):
        def adapter(plan):
            self.fail('invalid request reached adapter')
        for change in ({'requires_confirmation': False}, {'id': '../app.nro'}, {'backend': 'bsd'}, {'request_id': 'x'}, {'verb': 'unknown'}):
            events, errors = self.run_peer(adapter, **change)
            self.assertFalse(events)
            self.assertTrue(errors)

    def test_missing_exit_or_illegal_event(self):
        for sequence in ([('accepted', None)], [('exited', 0)], [('accepted', None), ('running', None), ('exited', True)]):
            events, errors = self.run_peer(lambda plan: iter(sequence))
            self.assertEqual(events[-1]['state'], 'failed')
            self.assertTrue(errors)


if __name__ == '__main__':
    unittest.main()

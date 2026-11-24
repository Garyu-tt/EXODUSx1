import os
import pathlib
import socket
import ssl
import subprocess
import tempfile
import time
import unittest

import exr_client as client
from test_loopback import certs


class ClientTests(unittest.TestCase):
    def test_real_tls(self):
        server = str(pathlib.Path(os.environ['EXR_SERVER']).resolve())
        cwd = os.getcwd()
        with tempfile.TemporaryDirectory() as d:
            os.chdir(d)
            try:
                certs()
                p = subprocess.Popen([server, 'ca.pem', 'server.pem', 'server.key', 'client.pem'], stdout=subprocess.PIPE, text=True)
                try:
                    line = p.stdout.readline()
                    self.assertTrue(line.startswith('READY '), line)
                    port = int(line.split()[1])
                    args = (port, 'localhost', 'ca.pem', 'client.pem', 'client.key')
                    self.assertEqual(client.request(*args, 'system.status'), (0, b'scope=host_reference\nhardware_verified=false\n'))
                    self.assertEqual(client.request(*args, 'battery.read'), (1, b''))
                    self.assertEqual(client.request(*args, 'capabilities.list'), (0, b'capabilities.list\nsystem.status\n'))
                    with self.assertRaises(ssl.SSLCertVerificationError):
                        client.request(port, 'wrong.example', *args[2:], 'system.status')
                    with self.assertRaises((OSError, EOFError)):
                        client.request(port, 'localhost', 'ca.pem', 'unpaired.pem', 'unpaired.key', 'system.status')
                finally:
                    p.terminate()
                    try:
                        p.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        p.kill()
                        p.wait()
                    p.stdout.close()
            finally:
                os.chdir(cwd)

    def test_invalid_headers(self):
        for header in [(b'NO', 1, 0, 0, 0), (b'EX', 2, 0, 0, 0), (b'EX', 1, 5, 0, 0), (b'EX', 1, 0, 512, 0), (b'EX', 1, 1, 1, 0), (b'EX', 1, 0, 0, 1)]:
            with self.subTest(header=header):
                a, b = socket.socketpair()
                with a, b:
                    b.sendall(client.HEADER.pack(*header))
                    with self.assertRaises(ValueError):
                        client.exchange(a, 'system.status', time.monotonic() + 1)
        a, b = socket.socketpair()
        with a, b:
            with self.assertRaises(TimeoutError):
                client.exchange(a, 'system.status', time.monotonic() - 1)
            with self.assertRaises(ValueError):
                client.exchange(a, 'shell.exec', time.monotonic() + 1)


if __name__ == '__main__':
    unittest.main()

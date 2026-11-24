import hashlib
import http.client
from http.server import HTTPServer
import json
import threading
import unittest
from relay import Cache, handler_for

class RelayTests(unittest.TestCase):
    def test_dedup_and_lru(self):
        c = Cache(6)
        a = c.put(b"abc")
        b = c.put(b"def")
        self.assertEqual(c.put(b"abc"), a)
        self.assertEqual(c.used, 6)
        c.put(b"ghi")
        self.assertIsNone(c.get(b))
        self.assertEqual(c.get(a), b"abc")
        self.assertEqual(a, hashlib.sha256(b"abc").hexdigest())

    def test_bounds(self):
        c = Cache(3)
        for data in (b"", b"abcd"):
            with self.assertRaises(ValueError):
                c.put(data)
        self.assertEqual(c.used, 0)

    def test_http(self):
        token = "a" * 32
        with HTTPServer(("127.0.0.1", 0), handler_for(Cache(32), token)) as server:
            worker = threading.Thread(target=server.serve_forever, daemon=True)
            worker.start()
            def request(method, path, body=None, auth=True):
                conn = http.client.HTTPConnection("127.0.0.1", server.server_port, timeout=3)
                try:
                    headers = {"Authorization": "Bearer " + token} if auth else {}
                    conn.request(method, path, body, headers)
                    resp = conn.getresponse()
                    return resp.status, resp.read()
                finally:
                    conn.close()
            try:
                self.assertEqual(request("GET", "/v1/status", auth=False)[0], 401)
                status, data = request("POST", "/v1/cache", b"texture")
                self.assertEqual(status, 201)
                key = json.loads(data)["sha256"]
                self.assertEqual(request("GET", "/v1/cache/" + key), (200, b"texture"))
                self.assertEqual(request("GET", "/v1/cache/" + "0"*64)[0], 404)
                self.assertEqual(request("POST", "/v1/cache", b"x"*33)[0], 413)
                self.assertEqual(json.loads(request("GET", "/v1/status")[1])["objects"], 1)
            finally:
                server.shutdown()
                worker.join(3)

if __name__ == "__main__":
    unittest.main()

"""GPU Relay R01: local authenticated content-addressed cache, not a game renderer."""
import argparse
from collections import OrderedDict
import hashlib
from http.server import BaseHTTPRequestHandler, HTTPServer
import hmac
import json
import os
import re
import socket

MAX_OBJECT = 8 * 1024 * 1024

class Cache:
    def __init__(self, budget, backend="ram"):
        if not isinstance(budget, int) or not 1 <= budget <= 1024 * 1024 * 1024:
            raise ValueError("invalid cache budget")
        self.budget = budget
        self.backend = backend
        self.entries = OrderedDict()
        self.used = 0
        self.cp = None
        if backend == "cuda":
            import cupy
            if cupy.cuda.runtime.getDeviceCount() < 1:
                raise RuntimeError("no CUDA GPU")
            self.cp = cupy
            # Dedicated pool: do not change global allocator.
            self.pool = cupy.cuda.MemoryPool()
            self.pool.set_limit(size=budget)
        elif backend != "ram":
            raise ValueError("unknown backend")

    def put(self, data):
        if not 0 < len(data) <= min(MAX_OBJECT, self.budget):
            raise ValueError("invalid object size")
        key = hashlib.sha256(data).hexdigest()
        if key in self.entries:
            self.entries.move_to_end(key)
            return key
        while self.used + len(data) > self.budget:
            _, (size, old) = self.entries.popitem(last=False)
            self.used -= size
            del old
        if self.cp is not None:
            # Allocate under dedicated pool; copy host bytes to device.
            with self.cp.cuda.using_allocator(self.pool.malloc):
                import numpy as np
                obj = self.cp.asarray(np.frombuffer(data, dtype=np.uint8))
                self.cp.cuda.get_current_stream().synchronize()
        else:
            obj = bytes(data)
        self.entries[key] = (len(data), obj)
        self.used += len(data)
        return key

    def get(self, key):
        entry = self.entries.get(key)
        if entry is None:
            return None
        self.entries.move_to_end(key)
        obj = entry[1]
        return obj.get().tobytes() if self.cp is not None else obj

def handler_for(cache, token):
    class Handler(BaseHTTPRequestHandler):
        def setup(self):
            super().setup()
            self.connection.settimeout(5)

        def log_message(self, *args):
            pass  # Do not log tokens, object identifiers or request data.

        def send_data(self, status, data, content_type="application/json"):
            self.send_response(status)
            self.send_header("Content-Type", content_type)
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(data)

        def reply(self, status, obj):
            self.send_data(status, json.dumps(obj).encode())

        def authorized(self):
            provided = self.headers.get("Authorization", "")
            if not hmac.compare_digest(provided.encode(), ("Bearer " + token).encode()):
                self.reply(401, {"error": "unauthorized"})
                return False
            return True

        def do_GET(self):
            if not self.authorized():
                return
            if self.path == "/v1/status":
                self.reply(200, {"backend": cache.backend, "used_bytes": cache.used,
                                 "budget_bytes": cache.budget, "objects": len(cache.entries)})
                return
            match = re.fullmatch(r"/v1/cache/([0-9a-f]{64})", self.path)
            if not match:
                self.reply(404, {"error": "unknown route"})
                return
            try:
                data = cache.get(match[1])
            except Exception:
                self.reply(503, {"error": "backend unavailable"})
                return
            if data is None:
                self.reply(404, {"error": "cache miss"})
            else:
                self.send_data(200, data, "application/octet-stream")

        def do_POST(self):
            if not self.authorized():
                return
            if self.path != "/v1/cache":
                self.reply(404, {"error": "unknown route"})
                return
            lengths = self.headers.get_all("Content-Length", [])
            if self.headers.get("Transfer-Encoding") or len(lengths) != 1:
                self.reply(400, {"error": "one content length required"})
                return
            if not re.fullmatch(r"[0-9]{1,10}", lengths[0]):
                self.reply(400, {"error": "invalid content length"})
                return
            size = int(lengths[0])
            if not 0 < size <= min(MAX_OBJECT, cache.budget):
                self.reply(413, {"error": "object size exceeds limit"})
                return
            try:
                data = self.rfile.read(size)
            except (OSError, socket.timeout):
                return
            if len(data) != size:
                self.reply(400, {"error": "incomplete object"})
                return
            try:
                key = cache.put(data)
            except Exception:
                self.reply(503, {"error": "backend unavailable"})
                return
            self.reply(201, {"sha256": key, "bytes": size})
    return Handler

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--backend", choices=["ram", "cuda"], default="ram")
    p.add_argument("--budget-mib", type=int, default=64)
    p.add_argument("--port", type=int, default=8765)
    args = p.parse_args()
    token = os.environ.get("EXODUS_RELAY_TOKEN", "")
    if len(token) < 32 or not token.isascii() or any(c.isspace() for c in token):
        p.error("set EXODUS_RELAY_TOKEN to a random ASCII token of at least 32 characters")
    cache = Cache(args.budget_mib * 1024 * 1024, args.backend)
    # Deliberate loopback-only R01. No plaintext LAN listener.
    with HTTPServer(("127.0.0.1", args.port), handler_for(cache, token)) as server:
        print("GPU Relay listening on loopback", flush=True)
        try:
            server.serve_forever()
        except KeyboardInterrupt:
            pass

if __name__ == "__main__":
    main()

# SPDX-License-Identifier: GPL-2.0-only
import hashlib
import io
import tempfile
import unittest
import urllib.error
from pathlib import Path
from unittest.mock import patch

import dependency_sources as subject


class Response(io.BytesIO):
    def geturl(self):
        return "https://example.invalid/source"


class DownloadRetryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.cache = Path(self.temp.name)
        self.data = b"pinned source"
        self.entry = {
            "path": "source/test.tar",
            "size": len(self.data),
            "sha256": hashlib.sha256(self.data).hexdigest(),
            "urls": ["https://example.invalid/source"],
        }
        self.sleep = patch.object(subject.time, "sleep").start()
        self.addCleanup(patch.stopall)

    def test_tls_timeout_then_success(self):
        with patch.object(subject.urllib.request, "urlopen", side_effect=[
            urllib.error.URLError(TimeoutError("TLS handshake timeout")),
            Response(self.data),
        ]) as fetch:
            result = subject.cached_file(self.entry, self.cache, False)
        self.assertEqual(fetch.call_count, 2)
        self.assertEqual(result.read_bytes(), self.data)
        self.assertEqual(list(self.cache.iterdir()), [result])
        self.sleep.assert_called_once_with(1)

    def test_exhausted(self):
        with patch.object(subject.urllib.request, "urlopen",
                          side_effect=TimeoutError("timeout")) as fetch:
            with self.assertRaisesRegex(ValueError, "attempt 3"):
                subject.cached_file(self.entry, self.cache, False)
        self.assertEqual(fetch.call_count, 3)
        self.assertEqual(list(self.cache.iterdir()), [])

    def test_integrity_failure_not_retried(self):
        with patch.object(subject.urllib.request, "urlopen",
                          return_value=Response(b"wrong source!")) as fetch:
            with self.assertRaisesRegex(ValueError, "SHA-256 mismatch"):
                subject.cached_file(self.entry, self.cache, False)
        self.assertEqual(fetch.call_count, 1)
        self.assertEqual(list(self.cache.iterdir()), [])
        self.sleep.assert_not_called()

    def test_offline_never_fetches(self):
        with patch.object(subject.urllib.request, "urlopen") as fetch:
            with self.assertRaisesRegex(ValueError, "offline cache"):
                subject.cached_file(self.entry, self.cache, True)
        fetch.assert_not_called()


if __name__ == "__main__":
    unittest.main()

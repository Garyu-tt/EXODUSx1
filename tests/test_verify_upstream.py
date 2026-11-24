import hashlib
from pathlib import Path
import tempfile
import unittest
from exodus.verify_upstream import verify


class UpstreamTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / 'asset.zip'
        self.path.write_bytes(b'abc')
        self.asset = {'bytes': 3, 'sha256': hashlib.sha256(b'abc').hexdigest()}

    def test_exact(self):
        self.assertEqual(verify(self.path, self.asset), self.asset)

    def test_same_size_tamper(self):
        self.path.write_bytes(b'abd')
        with self.assertRaises(ValueError):
            verify(self.path, self.asset)

    def test_size_mismatch(self):
        for data in (b'ab', b'abcd'):
            self.path.write_bytes(data)
            with self.assertRaises(ValueError):
                verify(self.path, self.asset)

    def test_invalid_lock(self):
        for asset in ({'bytes': True, 'sha256': self.asset['sha256']},
                      {'bytes': 3, 'sha256': 'bad'}):
            with self.assertRaises(ValueError):
                verify(self.path, asset)


if __name__ == '__main__':
    unittest.main()

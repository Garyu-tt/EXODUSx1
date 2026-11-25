import hashlib
from pathlib import Path
import stat
import tempfile
import unittest
import zipfile
from exodus.release_inventory import inventory


class InventoryTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / 'sd.zip'

    def write(self, entries):
        with zipfile.ZipFile(self.path, 'w') as archive:
            for name, payload in entries:
                archive.writestr(name, payload)

    def test_deterministic(self):
        self.write([('switch/b.nro', b'b'), ('atmosphere/a', b'a')])
        first = inventory(self.path)
        self.write([('atmosphere/a', b'a'), ('switch/b.nro', b'b')])
        self.assertEqual(first, inventory(self.path))
        self.assertEqual(first['total_bytes'], 2)
        self.assertEqual(first['files'][0]['sha256'], hashlib.sha256(b'a').hexdigest())

    def test_changed_payload(self):
        self.write([('switch/a.nro', b'a')])
        first = inventory(self.path)
        self.write([('switch/a.nro', b'b')])
        self.assertNotEqual(first, inventory(self.path))

    def test_unsafe(self):
        for name in ('../escape', '/absolute', 'a\\b', 'C:/a', 'a/../b', 'a/NUL.txt', 'a. '):
            with self.subTest(name=name):
                self.write([(name, b'a')])
                with self.assertRaises(ValueError):
                    inventory(self.path)

    def test_collision(self):
        for names in [('a', 'A'), ('x', 'x/y'), ('x/y', 'x')]:
            self.write([(n, b'a') for n in names])
            with self.assertRaises(ValueError):
                inventory(self.path)

    def test_symlink(self):
        entry = zipfile.ZipInfo('link')
        entry.create_system = 3
        entry.external_attr = (stat.S_IFLNK | 0o777) << 16
        self.write([(entry, b'target')])
        with self.assertRaises(ValueError):
            inventory(self.path)

    def test_directory(self):
        self.write([('switch/', b''), ('switch/a', b'a')])
        self.assertEqual(len(inventory(self.path)['files']), 1)


if __name__ == '__main__':
    unittest.main()

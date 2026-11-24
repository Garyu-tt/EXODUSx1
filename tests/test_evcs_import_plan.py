import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from collections import namedtuple
from exodus.evcs_import_plan import plan, FAT32_MAX

Usage = namedtuple('Usage', 'total used free')


class ImportPlanTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name).resolve()
        self.games = self.root / 'Games'
        self.games.mkdir()
        self.src = self.root / 'test.nsp'
        self.src.write_bytes(b'dummy')

    def test_copy_preflight_read_only(self):
        result = plan(self.src, self.games)
        self.assertEqual(result['status'], 'copy_preflight_ok')
        self.assertEqual(result['minimum_copy_payload_bytes'], 5)
        self.assertEqual(result['parts'], 1)
        self.assertFalse(result['writes_performed'])
        self.assertFalse(result['launch_authorized'])
        self.assertEqual(self.src.read_bytes(), b'dummy')
        self.assertEqual(list(self.games.iterdir()), [])

    def test_already_discoverable(self):
        target = self.games / self.src.name
        self.src.rename(target)
        result = plan(target, self.games)
        self.assertEqual(result['status'], 'already_discoverable')
        self.assertEqual(result['minimum_copy_payload_bytes'], 0)

    def test_collision_casefold(self):
        (self.games / 'TEST.NSP').write_bytes(b'existing')
        self.assertEqual(plan(self.src, self.games)['status'], 'destination_exists')
        self.assertEqual((self.games / 'TEST.NSP').read_bytes(), b'existing')

    def test_space(self):
        with patch('exodus.evcs_import_plan.shutil.disk_usage', return_value=Usage(100, 96, 4)):
            self.assertEqual(plan(self.src, self.games)['status'], 'insufficient_copy_space')

    def test_fat32(self):
        with self.src.open('wb') as f:
            f.truncate(FAT32_MAX + 1)
        self.assertEqual(plan(self.src, self.games, True)['status'], 'fat32_file_too_large')

    def test_split(self):
        self.src.unlink()
        self.src.mkdir()
        (self.src / '00').write_bytes(b'1234')
        (self.src / '01').write_bytes(b'12')
        result = plan(self.src, self.games, True)
        self.assertEqual((result['parts'], result['logical_bytes']), (2, 6))
        (self.src / '01').rename(self.src / '02')
        with self.assertRaises(ValueError):
            plan(self.src, self.games)

    def test_symlink(self):
        link = self.root / 'link.nsp'
        try:
            link.symlink_to(self.src)
        except (OSError, NotImplementedError):
            self.skipTest('symlinks unavailable')
        with self.assertRaises(ValueError):
            plan(link, self.games)

    def test_empty_unsupported_and_missing(self):
        self.src.write_bytes(b'')
        with self.assertRaises(ValueError):
            plan(self.src, self.games)
        bad = self.root / 'bad.zip'
        bad.write_bytes(b'1')
        with self.assertRaises(ValueError):
            plan(bad, self.games)
        with self.assertRaises(OSError):
            plan(self.src, self.root / 'missing')


if __name__ == '__main__':
    unittest.main()

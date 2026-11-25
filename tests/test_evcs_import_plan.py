import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from collections import namedtuple
from types import SimpleNamespace
from exodus.evcs_import_plan import plan, FAT32_MAX, is_indirect

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
        # Avoid allocating 4 GiB on CI filesystems without sparse-file support.
        item = SimpleNamespace(st_size=FAT32_MAX + 1, st_dev=self.src.stat().st_dev)
        with patch('exodus.evcs_import_plan.inventory', return_value=[item]):
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

    def test_parent_traversal_is_not_normalized_away(self):
        nested = self.root / 'nested'
        nested.mkdir()
        with self.assertRaises(ValueError):
            plan(nested / '..' / self.src.name, self.games)
        with self.assertRaises(ValueError):
            plan(self.src, nested / '..' / 'Games')
        self.assertEqual(list(self.games.iterdir()), [])

    def test_reparse_attribute_rejected_even_for_regular_mode(self):
        import stat
        self.assertTrue(is_indirect(SimpleNamespace(
            st_mode=stat.S_IFREG, st_file_attributes=0x400)))
        self.assertTrue(is_indirect(SimpleNamespace(
            st_mode=stat.S_IFDIR, st_file_attributes=0x400)))
        self.assertFalse(is_indirect(SimpleNamespace(st_mode=stat.S_IFREG)))

    def test_indirect_parent_and_split_member(self):
        nested = self.root / 'nested'
        nested.mkdir()
        game = nested / 'game.nsp'
        game.write_bytes(b'1234')
        link = self.root / 'alias'
        try:
            link.symlink_to(nested, target_is_directory=True)
        except (OSError, NotImplementedError):
            self.skipTest('symlinks unavailable')
        with self.assertRaises(ValueError):
            plan(link / game.name, self.games)
        split = self.root / 'split.nsp'
        split.mkdir()
        (split / '00').write_bytes(b'1234')
        (split / '01').symlink_to(game)
        with self.assertRaises(ValueError):
            plan(split, self.games)
        self.assertEqual(list(self.games.iterdir()), [])

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

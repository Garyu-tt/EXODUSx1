import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import os
import sys
from exodus.evcs_transfer import transfer, publish


@unittest.skipUnless(os.name == 'nt' or sys.platform.startswith('linux'), 'publication backend unsupported')
class TransferTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name).resolve()
        self.games = self.root / 'Games'
        self.games.mkdir()
        self.src = self.root / 'test.nsp'
        self.src.write_bytes(b'dummy' * 1000)
        self.dest = self.games / self.src.name

    def test_copy_and_recover_published(self):
        self.assertEqual(transfer(self.src, self.games)['status'], 'published_verified')
        self.assertEqual(self.dest.read_bytes(), self.src.read_bytes())
        self.assertEqual(transfer(self.src, self.games, resume=True)['status'], 'published_verified')

    def test_partial_recovery(self):
        def interrupt(src, dst, expected):
            dst.write_bytes(b'partial')
            raise OSError('interrupted')
        with patch('exodus.evcs_transfer.write_copy', side_effect=interrupt), self.assertRaises(OSError):
            transfer(self.src, self.games)
        self.assertFalse(self.dest.exists())
        with self.assertRaises(ValueError):
            transfer(self.src, self.games)
        transfer(self.src, self.games, resume=True)
        self.assertEqual(self.dest.read_bytes(), self.src.read_bytes())

    def test_split_and_resume_before_publish(self):
        self.src.unlink()
        self.src.mkdir()
        (self.src / '00').write_bytes(b'1234')
        (self.src / '01').write_bytes(b'12')
        with patch('exodus.evcs_transfer.publish', side_effect=OSError('interrupt')), self.assertRaises(OSError):
            transfer(self.src, self.games, True)
        self.assertFalse(self.dest.exists())
        with patch('exodus.evcs_transfer.write_copy', side_effect=AssertionError('must reuse verified parts')):
            transfer(self.src, self.games, True, True)
        self.assertEqual((self.dest / '00').read_bytes(), b'1234')
        self.assertEqual((self.dest / '01').read_bytes(), b'12')
        self.assertTrue((self.src / '00').exists())

    def test_collision_and_no_replace(self):
        self.dest.write_bytes(b'existing')
        with self.assertRaises(ValueError):
            transfer(self.src, self.games)
        with self.assertRaises(OSError):
            publish(self.src, self.dest)
        self.assertEqual(self.dest.read_bytes(), b'existing')
        self.assertTrue(self.src.exists())

    def test_source_change_blocks_recovery(self):
        with patch('exodus.evcs_transfer.publish', side_effect=OSError('interrupt')), self.assertRaises(OSError):
            transfer(self.src, self.games)
        self.src.write_bytes(b'new')
        with self.assertRaisesRegex(ValueError, 'journal mismatch'):
            transfer(self.src, self.games, resume=True)
        self.assertFalse(self.dest.exists())
        self.assertEqual(self.src.read_bytes(), b'new')

    def test_metadata_not_separated(self):
        sidecar = Path(str(self.src) + '.evcs')
        sidecar.write_text('owner metadata')
        with self.assertRaises(ValueError):
            transfer(self.src, self.games)
        self.assertFalse(self.dest.exists())
        self.assertTrue(sidecar.exists())

    def test_hardlinked_partial_is_not_unlinked_or_published(self):
        with patch('exodus.evcs_transfer.publish', side_effect=OSError('interrupt')), self.assertRaises(OSError):
            transfer(self.src, self.games)
        stage = next(self.games.glob('.evcs-stage-*'))
        payload = stage / 'payload'
        payload.unlink()
        try:
            os.link(self.src, payload)
        except (OSError, NotImplementedError):
            self.skipTest('hardlinks unavailable')
        with self.assertRaisesRegex(ValueError, 'unexpected staging file'):
            transfer(self.src, self.games, resume=True)
        self.assertTrue(payload.exists())
        self.assertTrue(self.src.exists())
        self.assertFalse(self.dest.exists())

    def test_indirect_stage_directory_is_rejected(self):
        with patch('exodus.evcs_transfer.publish', side_effect=OSError('interrupt')), self.assertRaises(OSError):
            transfer(self.src, self.games)
        stage = next(self.games.glob('.evcs-stage-*'))
        elsewhere = self.root / 'elsewhere'
        stage.rename(elsewhere)
        try:
            stage.symlink_to(elsewhere, target_is_directory=True)
        except (OSError, NotImplementedError):
            self.skipTest('symlinks unavailable')
        before = (elsewhere / 'payload').read_bytes()
        with self.assertRaises(ValueError):
            transfer(self.src, self.games, resume=True)
        self.assertEqual((elsewhere / 'payload').read_bytes(), before)
        self.assertFalse(self.dest.exists())

    def test_empty_partial_is_recovered(self):
        def interrupt(src, dst, expected):
            dst.touch()
            raise OSError('interrupted')
        with patch('exodus.evcs_transfer.write_copy', side_effect=interrupt), self.assertRaises(OSError):
            transfer(self.src, self.games)
        self.assertEqual(transfer(self.src, self.games, resume=True)['status'], 'published_verified')
        self.assertEqual(self.dest.read_bytes(), self.src.read_bytes())

    def test_already_discoverable(self):
        self.src.rename(self.dest)
        self.assertEqual(transfer(self.dest, self.games)['status'], 'already_discoverable')
        self.assertEqual(list(self.games.iterdir()), [self.dest])


if __name__ == '__main__':
    unittest.main()

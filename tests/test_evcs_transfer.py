import hashlib
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import os
import sys
import stat
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
        # Link an independent file with identical content, so changing its
        # link count cannot invalidate the original source fingerprint.
        external = self.root / 'independent.bin'
        external.write_bytes(self.src.read_bytes())
        try:
            os.link(external, payload)
        except (OSError, NotImplementedError):
            self.skipTest('hardlinks unavailable')
        with self.assertRaisesRegex(ValueError, 'unexpected staging file'):
            transfer(self.src, self.games, resume=True)
        self.assertTrue(payload.exists())
        self.assertTrue(self.src.exists())
        self.assertFalse(self.dest.exists())
        self.assertEqual(external.read_bytes(), self.src.read_bytes())
        self.assertTrue(os.path.samefile(external, payload))

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

    def test_resume_retries_journal_and_payload_file_sync_failures(self):
        for split in (False, True):
            # Fail the journal or either split payload after the complete bytes
            # have been flushed into the OS cache but before fsync succeeds.
            for failed_file in range(1, 4 if split else 3):
                with self.subTest(split=split, failed_file=failed_file), tempfile.TemporaryDirectory() as tmp:
                    root = Path(tmp).resolve()
                    games = root / 'Games'
                    games.mkdir()
                    source = root / 'game.nsp'
                    if split:
                        source.mkdir()
                        (source / '00').write_bytes(b'1234')
                        (source / '01').write_bytes(b'12')
                    else:
                        source.write_bytes(b'1234')
                    real_sync = os.fsync
                    calls = 0

                    def interrupt(fd):
                        nonlocal calls
                        if stat.S_ISREG(os.fstat(fd).st_mode):
                            calls += 1
                            if calls == failed_file:
                                raise OSError('injected file sync failure')
                        return real_sync(fd)

                    with patch('exodus.evcs_transfer.os.fsync', side_effect=interrupt), \
                            self.assertRaisesRegex(OSError, 'injected file sync failure'):
                        transfer(source, games)
                    stage = next(games.glob('.evcs-stage-*'))
                    target = stage / 'journal.json' if failed_file == 1 else (
                        stage / 'payload' / f'{failed_file - 2:02d}' if split else stage / 'payload')
                    original = target.read_bytes()
                    identity = target.stat()

                    def fail_again(fd):
                        info = os.fstat(fd)
                        if (info.st_dev, info.st_ino) == (identity.st_dev, identity.st_ino):
                            raise OSError('same file still cannot sync')
                        return real_sync(fd)

                    with patch('exodus.evcs_transfer.os.fsync', side_effect=fail_again), \
                            patch('exodus.evcs_transfer.publish') as publication, \
                            self.assertRaisesRegex(OSError, 'same file still cannot sync'):
                        transfer(source, games, resume=True)
                    publication.assert_not_called()
                    self.assertFalse((games / source.name).exists())
                    self.assertEqual(target.read_bytes(), original)
                    synced = []

                    def observe_sync(fd):
                        info = os.fstat(fd)
                        synced.append((info.st_dev, info.st_ino))
                        return real_sync(fd)

                    with patch('exodus.evcs_transfer.os.fsync', side_effect=observe_sync):
                        self.assertEqual(transfer(source, games, resume=True)['status'], 'published_verified')
                    self.assertIn((identity.st_dev, identity.st_ino), synced)
                    self.assertTrue(source.exists())
                    if split:
                        for name in ('00', '01'):
                            self.assertEqual((games / source.name / name).read_bytes(), (source / name).read_bytes())
                    else:
                        self.assertEqual((games / source.name).read_bytes(), source.read_bytes())

    def test_already_discoverable(self):
        self.src.rename(self.dest)
        self.assertEqual(transfer(self.dest, self.games)['status'], 'already_discoverable')
        self.assertEqual(list(self.games.iterdir()), [self.dest])


if __name__ == '__main__':
    unittest.main()

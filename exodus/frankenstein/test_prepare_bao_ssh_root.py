import contextlib
import io
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import prepare_bao_ssh_root as subject


class StageTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.base = Path(self.tmp.name) / 'base'
        self.root = Path(self.tmp.name) / 'root'
        self.outside = Path(self.tmp.name) / 'outside'
        for directory in (self.base, self.root, self.outside):
            directory.mkdir()
        for rel in subject.PROGRAMS:
            self.put(rel, b'\x7fELF fixture')
        for rel in subject.CONFIG:
            self.put(rel, b'config')

    def put(self, rel, content):
        path = self.base / rel
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    def readelf(self, args, **kwargs):
        return 'Machine: AArch64\n'

    def stage(self):
        with patch.object(subject.subprocess, 'check_output', side_effect=self.readelf):
            with contextlib.redirect_stdout(io.StringIO()):
                subject.stage(self.base, self.root)

    def test_valid(self):
        self.stage()
        self.assertEqual((self.root / subject.PROGRAMS[0]).read_bytes(), b'\x7fELF fixture')

    def test_nonelf_program(self):
        self.put(subject.PROGRAMS[0], b'not an executable')
        with self.assertRaisesRegex(ValueError, 'not ELF'):
            self.stage()
        self.assertFalse((self.root / subject.PROGRAMS[0]).exists())

    def test_wrong_architecture(self):
        self.readelf = lambda *a, **kw: 'Machine: X86-64\n'
        with self.assertRaisesRegex(ValueError, 'non-AArch64'):
            self.stage()
        self.assertFalse((self.root / subject.PROGRAMS[0]).exists())

    def test_missing_dependency_no_partial_stage(self):
        self.readelf = lambda *a, **kw: 'Machine: AArch64\n(NEEDED) [libmissing.so.1]\n'
        with self.assertRaisesRegex(ValueError, 'missing DT_NEEDED'):
            self.stage()
        self.assertFalse((self.root / subject.PROGRAMS[0]).exists())

    def test_destination_symlink_no_external_mkdir(self):
        (self.root / 'usr').symlink_to(self.outside, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'destination'):
            self.stage()
        self.assertEqual(list(self.outside.iterdir()), [])

    def test_dependency_closure(self):
        self.put('usr/lib/private/libfixture.so.1', b'\x7fELF lib')
        self.readelf = lambda args, **kw: ('Machine: AArch64\n' +
            ('' if args[-1].endswith('libfixture.so.1') else '(NEEDED) [libfixture.so.1]\n'))
        self.stage()
        self.assertTrue((self.root / 'usr/lib/private/libfixture.so.1').is_file())


if __name__ == '__main__':
    unittest.main()

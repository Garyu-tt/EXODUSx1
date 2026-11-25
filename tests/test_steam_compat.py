import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from exodus.steam_compat import candidate_manifests, invocation, read_target


class SteamCompatTest(unittest.TestCase):
    def setUp(self):
        self.target = {'version': 1, 'backend': 'horizon', 'kind': 'title_id', 'id': '010000000000ABCD'}

    def test_title(self):
        result = invocation('exodus-horizon', 'waitforexitandrun', self.target)
        self.assertEqual(result['argv'], ['--native-target', 'switch', '010000000000abcd'])
        self.assertTrue(result['requires_exit_tracking'])
        self.assertFalse(result['executed'])
        self.assertEqual(result['status'], 'bridge_unavailable')

    def test_nro(self):
        self.target.update(kind='nro', id='tools/My App.nro')
        result = invocation('exodus-horizon', 'run', self.target)
        self.assertEqual(result['argv'][-1], 'tools/My App.nro')
        self.assertFalse(result['requires_exit_tracking'])

    def test_backend_and_verb(self):
        for tool, verb in [('exodus-bsd', 'run'), ('proton', 'run'), ('exodus-horizon', 'getcompatpath')]:
            with self.assertRaises(ValueError):
                invocation(tool, verb, self.target)
        with self.assertRaisesRegex(ValueError, 'unavailable'):
            invocation('exodus-bsd', 'run', dict(self.target, backend='bsd', kind='elf'))

    def test_invalid_targets(self):
        for kind, ident in [('nro', '../a.nro'), ('nro', 'a.nsp'), ('title_id', '0' * 16), ('elf', '/bin/sh')]:
            with self.assertRaises(ValueError):
                invocation('exodus-horizon', 'run', dict(self.target, kind=kind, id=ident))

    def test_marker(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / 'a.exodus-target'
            path.write_text(json.dumps(self.target))
            self.assertEqual(read_target(path), self.target)
            for data in ('{"version":1,"version":1}', 'x' * 4097, json.dumps(dict(self.target, version=True))):
                path.write_text(data)
                with self.assertRaises(ValueError):
                    read_target(path)

    def test_direct_schema(self):
        for target in (None, [], dict(self.target, version=True),
                       dict(self.target, version=2), dict(self.target, extra=1),
                       dict(self.target, backend=[]), dict(self.target, id=None)):
            with self.subTest(target=target), self.assertRaises(ValueError):
                invocation('exodus-horizon', 'run', target)

    def test_marker_schema_and_directory(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / 'a.exodus-target'
            path.mkdir()
            with self.assertRaises(ValueError):
                read_target(path)
            path.rmdir()
            path.write_text(json.dumps(dict(self.target, kind=[])))
            with self.assertRaises(ValueError):
                read_target(path)

    @unittest.skipUnless(hasattr(os, 'mkfifo') and hasattr(os, 'O_NONBLOCK'), 'POSIX FIFO test')
    def test_marker_substitution(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / 'a.exodus-target'
            path.write_text(json.dumps(self.target))
            original_open = os.open

            def substitute(name, flags):
                path.unlink()
                os.mkfifo(path)
                return original_open(name, flags)

            with patch('exodus.steam_compat.os.open', side_effect=substitute):
                with self.assertRaises(ValueError):
                    read_target(path)
            with self.assertRaises(ValueError):
                read_target(path)

    @unittest.skipUnless(hasattr(os, 'O_NOFOLLOW'), 'POSIX no-follow test')
    def test_marker_symlink(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / 'a.exodus-target'
            original = Path(root) / 'original'
            original.write_text(json.dumps(self.target))
            path.symlink_to(original)
            with self.assertRaises(ValueError):
                read_target(path)

    def test_manifest_preview(self):
        result = candidate_manifests('exodus-horizon', 'horizon', 'freebsd')
        self.assertFalse(result['installable'])
        self.assertIn('"commandline" "/exodus-compat %verb%"', result['toolmanifest.vdf'])
        self.assertIn('"from_oslist" "horizon"', result['compatibilitytool.vdf'])
        for source in ('windows', 'horizon"', ''):
            with self.assertRaises(ValueError):
                candidate_manifests('exodus-horizon', source, 'freebsd')
        with self.assertRaises(ValueError):
            candidate_manifests('exodus-bsd', 'freebsd', 'freebsd')


if __name__ == '__main__':
    unittest.main()

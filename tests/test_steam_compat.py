import json
from pathlib import Path
import tempfile
import unittest

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

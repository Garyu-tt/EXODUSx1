import json
from pathlib import Path
import tempfile
import unittest
from exodus.native_descriptors import descriptor, export


class NativeDescriptorTest(unittest.TestCase):
    def test_native_only(self):
        d = descriptor('switch', '010000000000ABCD')
        self.assertEqual(d['argv'], ['--native-target', 'switch', '010000000000abcd'])
        self.assertTrue(d['requires_confirmation'])
        self.assertEqual(d['launch_status'], 'unverified')
        for provider in ('steam', 'other', 'corkdeck'):
            self.assertIsNone(descriptor(provider, '010000000000abcd'))

    def test_homebrew(self):
        for target in ('tools/demo.nro', 'My Demo.NRO'):
            self.assertEqual(descriptor('switch_homebrew', target)['argv'][-1], target)
        for target in ('', '../a.nro', 'sdmc:/switch/a.nro', 'a/../b.nro', 'a.nsp', 'sdmc:/Games/My Demo.NRO',
                       'a".nro', 'a\\b.nro', 'a//b.nro', '/a.nro', 'a\x00.nro', '\ud800.nro'):
            self.assertIsNone(descriptor('switch_homebrew', target))
        self.assertIsNone(descriptor('switch_homebrew', 'x' * 500 + '.nro'))
        self.assertIsNone(descriptor('switch', '0000000000000000'))

    def test_export_dedup(self):
        with tempfile.TemporaryDirectory() as root:
            path = Path(root) / 'catalog.json'
            def entry(provider, ident):
                return dict(provider=provider, provider_id=ident, name='Demo', artwork=None,
                            installation_state='unknown', compatibility_state='unknown',
                            launch_mode=None, last_played=None)
            entries = [entry('switch', x) for x in
                       ('010000000000ABCD', '010000000000abcd')]
            entries.append(entry('other', 'Game.nsp'))
            path.write_text(json.dumps({'schema_version': 1, 'entries': entries}))
            self.assertEqual(len(export(path)['descriptors']), 1)
            for invalid in ([entries[0], entries[0]], [{'provider': 'switch', 'provider_id': '010000000000abcd'}]):
                path.write_text(json.dumps({'schema_version': 1, 'entries': invalid}))
                with self.assertRaises(ValueError):
                    export(path)
            path.write_text('{"schema_version":1,"schema_version":1,"entries":[]}')
            with self.assertRaises(ValueError):
                export(path)
            path.write_text('{"schema_version":true,"entries":[]}')
            with self.assertRaises(ValueError):
                export(path)


if __name__ == '__main__':
    unittest.main()

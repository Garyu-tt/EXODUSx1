"""Keep the pinned runtime's byte-oriented program identities during import."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from exodus.corkdeck_catalog import parse_catalog


REPO = Path(__file__).resolve().parents[1]


def source_catalog(paths, version):
    return f"version={version}\nnext-id={len(paths) + 1}\n" + "".join(
        f"[game {number}]\npath={path}\ntitle=Program {number}\n"
        for number, path in enumerate(paths, 1)
    )


class CorkDeckIdentityTests(unittest.TestCase):
    def test_unicode_folding_does_not_merge_distinct_programs(self):
        # The pinned donor's strcasecmp keeps these different UTF-8 byte strings.
        pairs = (
            ("Straße", "STRASSE"),
            ("ẞ", "ß"),
            ("Σ", "σ"),
            ("σ", "ς"),
            ("K", "k"),
            ("ﬀ", "ff"),
            ("İ", "i\u0307"),
            ("Ä", "ä"),
            ("А", "а"),
        )
        for version in (2, 3):
            for first, second in pairs:
                paths = [f"sdmc:/switch/wine/{name}.exe" for name in (first, second)]
                with self.subTest(version=version, paths=paths):
                    self.assertEqual(paths[0].casefold(), paths[1].casefold())
                    entries = parse_catalog(source_catalog(paths, version))
                    self.assertEqual([entry["provider_id"] for entry in entries], paths)
                    self.assertEqual([entry["name"] for entry in entries], ["Program 1", "Program 2"])

    def test_ascii_duplicates_are_rejected_after_percent_decoding(self):
        first = "sdmc:/switch/wine/Straße.exe"
        duplicates = (
            first,
            "SDMC:/SWITCH/WINE/STRAßE.EXE",
            "sdmc:/switch/wine/%53tra%C3%9Fe.exe",
            "%73dmc:/switch/wine/stra%C3%9Fe.EXE",
        )
        for version in (2, 3):
            for duplicate in duplicates:
                with self.subTest(version=version, duplicate=duplicate):
                    with self.assertRaisesRegex(ValueError, "duplicate program path"):
                        parse_catalog(source_catalog([first, duplicate], version))

    def test_percent_encoded_unicode_stays_distinct(self):
        for version in (2, 3):
            with self.subTest(version=version):
                source = source_catalog([
                    "sdmc:/switch/wine/Stra%C3%9Fe.exe",
                    "sdmc:/switch/wine/STRASSE.exe",
                ], version)
                entries = parse_catalog(source)
                self.assertEqual([entry["provider_id"] for entry in entries], [
                    "sdmc:/switch/wine/Straße.exe", "sdmc:/switch/wine/STRASSE.exe",
                ])

    def test_invalid_utf8_paths_remain_rejected(self):
        for suffix in ("%FF", "%C0%AF", "%ED%A0%80"):
            with self.subTest(suffix=suffix):
                with self.assertRaises(UnicodeDecodeError):
                    parse_catalog(source_catalog([f"sdmc:/switch/wine/{suffix}.exe"], 3))

    def test_cli_preserves_both_programs_and_the_source(self):
        paths = ["sdmc:/switch/wine/Straße.exe", "sdmc:/switch/wine/STRASSE.exe"]
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "launcher-library-v2.ini"
            for version in (2, 3):
                with self.subTest(version=version):
                    original = source_catalog(paths, version).encode("utf-8")
                    source.write_bytes(original)
                    result = subprocess.run(
                        [sys.executable, "-B", "-X", "utf8", "-m", "exodus.corkdeck_catalog", str(source)],
                        cwd=REPO, capture_output=True, text=True, encoding="utf-8", check=False,
                    )
                    self.assertEqual((result.returncode, result.stderr), (0, ""))
                    document = json.loads(result.stdout)
                    self.assertEqual(document["schema_version"], 1)
                    self.assertEqual([entry["provider_id"] for entry in document["entries"]], paths)
                    self.assertEqual(source.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()

"""Exercise the combined catalog command with offline Steam and NRO fixtures."""

import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib


REPO = Path(__file__).resolve().parents[1]


def pack_titles(nacp, titles):
    compressor = zlib.compressobj(wbits=-zlib.MAX_WBITS)
    packed = compressor.compress(titles) + compressor.flush()
    struct.pack_into("<H", nacp, 0, len(packed))
    nacp[2:2 + len(packed)] = packed
    nacp[0x3215] = 1


def nro_image(title=None, *, locale=0, compressed=False):
    code = bytearray(0x100)
    struct.pack_into("<4sII", code, 0x10, b"NRO0", 0, len(code))
    if title is None:
        return bytes(code)

    nacp = bytearray(0x4000)
    titles = bytearray(0x6000 if compressed else 0x3000)
    name = title.encode("utf-8")
    start = locale * 0x300
    titles[start:start + len(name)] = name
    titles[start + 0x200:start + 0x206] = b"Author"
    if compressed:
        pack_titles(nacp, titles)
    else:
        nacp[:len(titles)] = titles
    assets = struct.pack("<4sI6Q", b"ASET", 0, 0, 0, 0x38, len(nacp), 0, 0)
    return bytes(code) + assets + nacp


class AppCatalogTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.sd = self.root / "SD"
        self.switch = self.sd / "switch"
        self.switch.mkdir(parents=True)
        self.steam = self.root / "Steam" / "steamapps"
        self.steam.mkdir(parents=True)

    def run_catalog(self, *args):
        return subprocess.run(
            [sys.executable, "-B", "-X", "utf8", "-m", "exodus.app_catalog", *map(str, args)],
            cwd=REPO, capture_output=True, text=True, encoding="utf-8", check=False,
        )

    def add_nro(self, relative, data):
        path = self.switch / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def add_steam_manifest(self):
        (self.steam / "appmanifest_42.acf").write_text(
            'AppState { appid 42 name "Example" StateFlags 4 }', encoding="utf-8",
        )

    def test_combines_steam_and_homebrew_with_provider_ids(self):
        self.add_steam_manifest()
        self.add_nro("tools/example/local.NRO", nro_image("Приклад", locale=12))
        self.add_nro("Example.nro", nro_image("Example"))
        self.add_nro("ignored.txt", b"not an application")
        (self.sd / "outside.nro").write_bytes(nro_image("Outside switch directory"))

        result = self.run_catalog("--switch-sd", self.sd, "--steam-library", self.steam.parent)
        self.assertEqual((result.returncode, result.stderr), (0, ""))
        catalog = json.loads(result.stdout)
        self.assertEqual(catalog["schema_version"], 1)
        self.assertEqual(
            [(item["provider"], item["provider_id"], item["name"]) for item in catalog["entries"]],
            [
                ("steam", "42", "Example"),
                ("switch_homebrew", "Example.nro", "Example"),
                ("switch_homebrew", "tools/example/local.NRO", "Приклад"),
            ],
        )
        self.assertEqual(catalog["entries"][-1], {
            "provider": "switch_homebrew",
            "provider_id": "tools/example/local.NRO",
            "name": "Приклад",
            "artwork": None,
            "installation_state": "reported_installed",
            "last_played": None,
            "compatibility_state": "unknown",
            "launch_mode": None,
        })
        self.assertNotIn(self.root.name, result.stdout)
        self.assertNotIn("Author", result.stdout)

    def test_homebrew_filename_fallback_when_no_embedded_title_is_available(self):
        self.add_nro("Blank title.nro", nro_image(""))
        self.add_nro("No assets.nro", nro_image())
        empty_assets = struct.pack("<4sI6Q", b"ASET", 0, 0, 0, 0, 0, 0, 0)
        self.add_nro("No NACP.nro", nro_image() + empty_assets)
        result = self.run_catalog("--switch-sd", self.sd)
        self.assertEqual((result.returncode, result.stderr), (0, ""))
        self.assertEqual(
            [item["name"] for item in json.loads(result.stdout)["entries"]],
            ["Blank title", "No NACP", "No assets"],
        )

    def test_steam_install_discovery_combines_with_homebrew_and_preserves_partial_results(self):
        self.add_steam_manifest()
        self.add_nro("Tools.nro", nro_image("Local tools"))
        config = self.steam / "libraryfolders.vdf"
        config.write_text("LibraryFolders {}", encoding="utf-8")
        result = self.run_catalog("--steam-install", self.steam.parent, "--switch-sd", self.sd)
        self.assertEqual((result.returncode, result.stderr), (0, ""))
        self.assertEqual(
            [(item["provider"], item["provider_id"]) for item in json.loads(result.stdout)["entries"]],
            [("steam", "42"), ("switch_homebrew", "Tools.nro")],
        )

        config.unlink()
        partial = self.run_catalog("--steam-install", self.steam.parent, "--switch-sd", self.sd)
        self.assertEqual(partial.returncode, 1)
        self.assertEqual(json.loads(partial.stdout), json.loads(result.stdout))
        self.assertIn("libraryfolders.vdf", partial.stderr)

    def test_reads_compressed_nacp_title_in_extended_language_slot(self):
        self.add_nro("Compressed.nro", nro_image("拡張タイトル", locale=23, compressed=True))
        result = self.run_catalog("--switch-sd", self.sd)
        self.assertEqual((result.returncode, result.stderr), (0, ""))
        self.assertEqual(
            [item["name"] for item in json.loads(result.stdout)["entries"]],
            ["拡張タイトル"],
        )

    def test_malformed_nros_report_partial_success_without_hiding_other_entries(self):
        invalid_size = bytearray(nro_image())
        struct.pack_into("<I", invalid_size, 0x18, 0xFFFFFFFF)
        invalid_offset = bytearray(nro_image("Broken"))
        struct.pack_into("<Q", invalid_offset, 0x118, 0xFFFFFFFFFFFFFFFF)
        invalid_title = bytearray(nro_image("Broken"))
        invalid_title[0x138] = 0xFF
        invalid_packed_size = bytearray(nro_image("Broken", compressed=True))
        struct.pack_into("<H", invalid_packed_size, 0x138, 0xFFFF)
        expanded_nacp = bytearray(0x4000)
        pack_titles(expanded_nacp, bytes(0x6001))
        invalid = {
            "short.nro": b"NRO0",
            "overlong.nro": invalid_size,
            "short-assets.nro": nro_image() + b"ASET",
            "outside.nro": invalid_offset,
            "truncated.nro": nro_image("Broken")[:-1],
            "invalid-title.nro": invalid_title,
            "oversize-compressed.nro": invalid_packed_size,
            "oversize-expanded.nro": nro_image("Broken")[:0x138] + expanded_nacp,
        }
        for path, image in invalid.items():
            self.add_nro(path, image)
        self.add_nro("intact.nro", nro_image("Readable"))
        self.add_steam_manifest()
        result = self.run_catalog("--steam-library", self.steam, "--switch-sd", self.sd)

        self.assertEqual(result.returncode, 1)
        self.assertEqual(
            [item["provider_id"] for item in json.loads(result.stdout)["entries"]],
            ["42", "intact.nro"],
        )
        for path in invalid:
            self.assertIn(path, result.stderr)
        self.assertNotIn("Traceback", result.stderr)

    @unittest.skipUnless(sys.platform.startswith("linux") and sys.getfilesystemencoding() == "utf-8",
                         "requires Linux byte filenames with UTF-8 filesystem decoding")
    def test_non_utf8_paths_leave_a_readable_partial_catalog(self):
        self.add_steam_manifest()
        self.add_nro("tools/Игры.nro", nro_image("Readable"))
        self.add_nro(os.fsdecode(b"invalid-\xfe.nro"), nro_image("Embedded title"))
        self.add_nro(os.fsdecode(b"bad-\xff/Fine.nro"), nro_image("Another title"))

        result = self.run_catalog("--switch-sd", self.sd, "--steam-library", self.steam)
        self.assertEqual(result.returncode, 1)
        self.assertEqual(
            [(item["provider"], item["provider_id"], item["name"])
             for item in json.loads(result.stdout)["entries"]],
            [("steam", "42", "Example"), ("switch_homebrew", "tools/Игры.nro", "Readable")],
        )
        self.assertEqual(result.stderr.count("NRO path is not valid UTF-8"), 2)
        self.assertIn(r"invalid-\udcfe.nro", result.stderr)
        self.assertIn(r"bad-\udcff/Fine.nro", result.stderr)

    def test_missing_switch_directory_preserves_readable_steam_entries(self):
        self.add_steam_manifest()
        result = self.run_catalog("--switch-sd", self.root / "missing", "--steam-library", self.steam)
        self.assertEqual(result.returncode, 1)
        self.assertIn("missing", result.stderr)
        self.assertEqual([item["provider_id"] for item in json.loads(result.stdout)["entries"]], ["42"])

    def test_empty_source_is_valid_but_omitting_all_sources_is_an_error(self):
        result = self.run_catalog("--switch-sd", self.sd)
        self.assertEqual((result.returncode, result.stderr), (0, ""))
        self.assertEqual(json.loads(result.stdout), {"schema_version": 1, "entries": []})
        result = self.run_catalog()
        self.assertEqual(result.returncode, 2)
        self.assertIn("supply at least one", result.stderr)


if __name__ == "__main__":
    unittest.main()

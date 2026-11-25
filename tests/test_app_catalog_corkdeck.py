"""Exercise CorkDeck sources through the combined catalog's public CLI."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tests.test_app_catalog import nro_image
from tests.test_catalog_output_limits import byte_boundary_entries


REPO = Path(__file__).resolve().parents[1]


class CombinedCorkDeckTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.steamapps = self.root / "Steam" / "steamapps"
        self.steamapps.mkdir(parents=True)
        self.sd = self.root / "SD"
        (self.sd / "switch").mkdir(parents=True)

    def command(self, *args):
        return subprocess.run(
            [sys.executable, "-B", "-X", "utf8", "-m", "exodus.app_catalog", *map(str, args)],
            cwd=REPO, capture_output=True, check=False,
        )

    def source(self, name, records, version=3):
        path = self.root / name
        text = f"version={version}\n"
        for index, record in enumerate(records, 1):
            text += f"[game {index}]\n"
            text += "".join(f"{key}={value}\n" for key, value in record.items())
        path.write_text(text, encoding="utf-8")
        return path

    def add_steam(self, name="Steam game"):
        (self.steamapps / "appmanifest_42.acf").write_text(
            f'AppState {{ appid 42 name "{name}" StateFlags 4 }}', encoding="utf-8",
        )

    def add_homebrew(self):
        (self.sd / "switch" / "Tools.nro").write_bytes(nro_image("Local tools"))

    def test_one_command_combines_three_providers_and_preserves_runtime_metadata(self):
        self.add_steam()
        self.add_homebrew()
        source = self.source("runtime.ini", [{
            "path": "sdmc:/switch/wine/A%25B.exe",
            "title": "Игра%3DOne",
            "portrait-art": "sdmc:/art/cover%20one.png",
            "landscape-art": "sdmc:/art/other.png",
            "launched-order": "12",
            "favorite": "1",
        }])
        result = self.command("--steam-library", self.steamapps, "--switch-sd", self.sd,
                              "--corkdeck-catalog", source)
        self.assertEqual((result.returncode, result.stderr), (0, b""))
        catalog = json.loads(result.stdout)
        self.assertEqual(catalog["schema_version"], 1)
        self.assertEqual([entry["provider"] for entry in catalog["entries"]],
                         ["steam", "switch_homebrew", "corkdeck"])
        self.assertEqual(catalog["entries"][2], {
            "provider": "corkdeck", "provider_id": "sdmc:/switch/wine/A%B.exe",
            "name": "Игра=One", "artwork": "sdmc:/art/cover one.png",
            "installation_state": "unknown", "last_played": None,
            "compatibility_state": "unknown", "launch_mode": None,
        })
        self.assertNotIn(str(source).encode(), result.stdout)
        self.assertNotIn(b"\r\n", result.stdout)
        result.stdout.decode("ascii")

    def test_runtime_source_works_alone_for_both_versions_and_empty_catalog(self):
        for version in (2, 3):
            with self.subTest(version=version):
                source = self.source(f"v{version}.ini", [{"path": "ums0:/Wine/Game.exe"}], version)
                result = self.command("--corkdeck-catalog", source)
                self.assertEqual((result.returncode, result.stderr), (0, b""))
                entry, = json.loads(result.stdout)["entries"]
                self.assertEqual((entry["provider"], entry["provider_id"], entry["name"]),
                                 ("corkdeck", "ums0:/Wine/Game.exe", "Game.exe"))
        empty = self.source("empty.ini", [])
        result = self.command("--corkdeck-catalog", empty)
        self.assertEqual((result.returncode, result.stderr), (0, b""))
        self.assertEqual(json.loads(result.stdout), {"schema_version": 1, "entries": []})

    def test_repeated_sources_and_case_aliases_keep_first_metadata_in_input_order(self):
        first = self.source("first.ini", [
            {"path": "sdmc:/Games/Игры.exe", "title": "First title", "square-art": "sdmc:/first.png"},
            {"path": "sdmc:/Games/Second.exe", "title": "Second game"},
        ])
        second = self.source("second.ini", [
            {"path": "SDMC:/GAMES/ИГРЫ.EXE", "title": "Stale title", "square-art": "sdmc:/stale.png"},
            {"path": "sdmc:/Games/Third.exe", "title": "Third game"},
        ])
        result = self.command("--corkdeck-catalog", first, "--corkdeck-catalog", second,
                              "--corkdeck-catalog", first)
        self.assertEqual((result.returncode, result.stderr), (0, b""))
        entries = json.loads(result.stdout)["entries"]
        self.assertEqual([entry["name"] for entry in entries], ["First title", "Second game", "Third game"])
        self.assertEqual(entries[0]["provider_id"], "sdmc:/Games/Игры.exe")
        self.assertEqual(entries[0]["artwork"], "sdmc:/first.png")
        reversed_result = self.command("--corkdeck-catalog", second, "--corkdeck-catalog", first)
        self.assertEqual((reversed_result.returncode, reversed_result.stderr), (0, b""))
        self.assertEqual([entry["name"] for entry in json.loads(reversed_result.stdout)["entries"]],
                         ["Stale title", "Third game", "Second game"])

    def test_identity_deduplication_stays_within_the_corkdeck_provider(self):
        self.add_steam()
        source = self.source("relative-runtime.ini", [{"path": "42", "title": "Runtime metadata"}])
        result = self.command("--steam-library", self.steamapps, "--corkdeck-catalog", source)
        self.assertEqual((result.returncode, result.stderr), (0, b""))
        self.assertEqual([(entry["provider"], entry["provider_id"]) for entry in json.loads(result.stdout)["entries"]],
                         [("steam", "42"), ("corkdeck", "42")])

    def test_bad_runtime_inputs_preserve_other_sources_and_later_runtime_catalogs(self):
        self.add_steam()
        self.add_homebrew()
        valid = self.source("valid.ini", [{"path": "sdmc:/Games/Valid.exe"}])
        missing = self.root / "missing.ini"
        directory = self.root / "directory.ini"
        directory.mkdir()
        malformed = self.source("malformed.ini", [{"path": "invalid%GG"}])
        unicode_error = self.root / "encoding.ini"
        unicode_error.write_bytes(b"version=3\n\xff")
        oversized = self.root / "large.ini"
        oversized.write_bytes(b"#" * (2 * 1024 * 1024 + 1))
        invalid_field = self.source("long-field.ini", [{"path": "a" * 512}])
        for invalid in (missing, directory, malformed, unicode_error, oversized, invalid_field):
            with self.subTest(input=invalid.name):
                result = self.command("--corkdeck-catalog", invalid, "--switch-sd", self.sd,
                                      "--steam-library", self.steamapps, "--corkdeck-catalog", valid)
                self.assertEqual(result.returncode, 1)
                self.assertIn(invalid.name.encode(), result.stderr)
                self.assertNotIn(b"Traceback", result.stderr)
                self.assertEqual([entry["provider"] for entry in json.loads(result.stdout)["entries"]],
                                 ["steam", "switch_homebrew", "corkdeck"])

    def test_invalid_catalog_does_not_leak_a_partially_parsed_game(self):
        bad = self.source("duplicate.ini", [{"path": "sdmc:/Partial.exe"}, {"path": "SDMC:/PARTIAL.EXE"}])
        result = self.command("--corkdeck-catalog", bad)
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"duplicate program path", result.stderr)
        self.assertEqual(json.loads(result.stdout)["entries"], [])

    def test_atomic_file_output_matches_stdout_including_readable_partial_results(self):
        source = self.source("runtime.ini", [{"path": "sdmc:/Game.exe", "title": "Runtime game"}])
        streamed = self.command("--corkdeck-catalog", source)
        output = self.root / "catalog.json"
        for partial in (False, True):
            with self.subTest(partial=partial):
                output.write_bytes(b"previous snapshot")
                extra = ("--corkdeck-catalog", self.root / "absent.ini") if partial else ()
                result = self.command("--corkdeck-catalog", source, *extra, "--output", output)
                self.assertEqual((result.returncode, result.stdout), (int(partial), b""))
                self.assertEqual(output.read_bytes(), streamed.stdout)
                self.assertEqual(list(self.root.glob(".exodus-catalog-*")), [])

    def test_failed_publication_preserves_destination_and_removes_staging_file(self):
        source = self.source("runtime.ini", [{"path": "sdmc:/Game.exe"}])
        output = self.root / "catalog.json"
        output.mkdir()
        result = self.command("--corkdeck-catalog", source, "--output", output)
        self.assertEqual((result.returncode, result.stdout), (1, b""))
        self.assertIn(b"previous output was preserved", result.stderr)
        self.assertTrue(output.is_dir())
        self.assertEqual(list(self.root.glob(".exodus-catalog-*")), [])

    def test_entry_budget_counts_all_sources_after_deduplication(self):
        args = []
        for group in range(16):
            source = self.source(f"runtime-{group}.ini", [
                {"path": f"sdmc:/Games/{group}/{entry}.exe"} for entry in range(256)
            ])
            args.extend(("--corkdeck-catalog", source))
        args.extend(args[:2])
        result = self.command(*args)
        self.assertEqual((result.returncode, result.stderr), (0, b""))
        self.assertEqual(len(json.loads(result.stdout)["entries"]), 4096)
        self.add_steam()
        output = self.root / "catalog.json"
        output.write_bytes(result.stdout)
        overflow = self.command(*args, "--steam-library", self.steamapps, "--output", output)
        self.assertEqual((overflow.returncode, overflow.stdout), (1, b""))
        self.assertIn(b"entry limit", overflow.stderr)
        self.assertEqual(output.read_bytes(), result.stdout)
        self.assertEqual(list(self.root.glob(".exodus-catalog-*")), [])

    def test_runtime_rows_count_toward_combined_byte_budget_before_output(self):
        self.add_steam(byte_boundary_entries()[0]["name"])
        source = self.source("runtime.ini", [{"path": "sdmc:/Game.exe"}])
        output = self.root / "catalog.json"
        output.write_bytes(b"previous snapshot")
        for destination in ((), ("--output", output)):
            with self.subTest(destination=bool(destination)):
                result = self.command("--steam-library", self.steamapps, "--corkdeck-catalog", source,
                                      *destination)
                self.assertEqual((result.returncode, result.stdout), (1, b""))
                self.assertIn(b"size limit", result.stderr)
                self.assertEqual(output.read_bytes(), b"previous snapshot")
                self.assertEqual(list(self.root.glob(".exodus-catalog-*")), [])


if __name__ == "__main__":
    unittest.main()

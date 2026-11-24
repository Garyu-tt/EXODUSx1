"""Exercise the user-facing Steam manifest importer in isolated libraries."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "exodus" / "steam_catalog.py"


class SteamCatalogTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.first = Path(temp.name) / "Primary" / "steamapps"
        self.second = Path(temp.name) / "Extra" / "steamapps"
        self.first.mkdir(parents=True)
        self.second.mkdir(parents=True)

    def scan(self, *paths):
        result = subprocess.run(
            [sys.executable, "-X", "utf8", str(SCRIPT), *map(str, paths)],
            capture_output=True, text=True, encoding="utf-8",
        )
        return result.returncode, json.loads(result.stdout), result.stderr

    def manifest(self, directory, app_id, name, flags=None):
        fields = f'"appid" "{app_id}"\n"name" "{name}"'
        if flags is not None:
            fields += f'\n"StateFlags" "{flags}"'
        (directory / f"appmanifest_{app_id}.acf").write_text(
            f'"AppState"\n{{\n{fields}\n}}\n', encoding="utf-8"
        )

    def test_reads_only_catalog_metadata_from_the_appstate_object(self):
        data = r'''// A Steam app manifest with nested metadata and quoted braces
        "AppState" {
            "appid" "42" "name" "Игра \"{A}\" \\ B"
            "LastOwner" "private-account-id" "LastPlayed" "1700000000"
            "InstalledDepots" { "1" { "name" "Different" "appid" "99" } }
            "StateFlags" "6"
        }'''
        (self.first / "appmanifest_42.acf").write_text(data, encoding="utf-8-sig")
        (self.first / "libraryfolders.vdf").write_bytes(b"ignored")
        code, output, errors = self.scan(self.first.parent)
        self.assertEqual((code, errors), (0, ""))
        self.assertEqual(output, {
            "schema_version": 1,
            "entries": [{
                "provider": "steam", "provider_id": "42", "name": 'Игра "{A}" \\ B',
                "artwork": None, "last_played": 1700000000,
                "installation_state": "reported_installed",
                "compatibility_state": "unknown", "launch_mode": None,
            }],
        })

    def test_merges_libraries_by_app_id_and_sorts_numerically(self):
        self.manifest(self.first, 10, "Stale", 1024)
        self.manifest(self.second, 10, "Current", 4)
        self.manifest(self.first, 2, "First copy", 4)
        self.manifest(self.second, 2, "Another copy", 4)
        self.manifest(self.first, 12, "Missing flags")
        self.manifest(self.second, 15, "Updating", 256)
        code, catalog, errors = self.scan(self.first, self.second.parent, self.first)
        self.assertEqual((code, errors), (0, ""))
        self.assertEqual(
            [(e["provider_id"], e["name"], e["installation_state"]) for e in catalog["entries"]],
            [("2", "First copy", "reported_installed"), ("10", "Current", "reported_installed"),
             ("12", "Missing flags", "unknown"), ("15", "Updating", "not_fully_installed")],
        )

    def test_rejects_corrupt_manifests_and_keeps_other_libraries(self):
        invalid = {
            10: b'AppState { appid 10 name "unfinished',
            11: b'AppState { appid 11 name X',
            12: b'AppState { appid 12 name X StateFlags -1 }',
            13: b'AppState { appid 77 name X }',
            14: b'AppState { appid 14 name { nested value } }',
            15: b'AppState { appid 15 name "\xff" }',
            16: b'AppState { appid 16 name X } }',
            4294967296: b'AppState { appid 4294967296 name X }',
        }
        for app_id, data in invalid.items():
            (self.first / f"appmanifest_{app_id}.acf").write_bytes(data)
        self.manifest(self.second, 42, "Readable")
        code, catalog, errors = self.scan(self.first.parent, self.second / "missing", self.second)
        self.assertEqual(code, 1)
        self.assertEqual([entry["provider_id"] for entry in catalog["entries"]], ["42"])
        for app_id in invalid:
            self.assertIn(f"appmanifest_{app_id}.acf", errors)
        self.assertIn("missing", errors)
        self.assertNotIn("Traceback", errors)

    def test_rejects_oversize_files_and_directories(self):
        path = self.first / "appmanifest_7.acf"
        with path.open("wb") as stream:
            stream.truncate(16 * 1024 * 1024 + 1)
        (self.first / "appmanifest_8.acf").mkdir()
        code, catalog, errors = self.scan(self.first)
        self.assertEqual((code, catalog["entries"]), (1, []))
        self.assertIn("manifest exceeds", errors)
        self.assertIn("expected a regular manifest", errors)

    def test_empty_steamapps_directory_is_valid(self):
        code, catalog, errors = self.scan(self.first)
        self.assertEqual((code, errors), (0, ""))
        self.assertEqual(catalog, {"schema_version": 1, "entries": []})


if __name__ == "__main__":
    unittest.main()

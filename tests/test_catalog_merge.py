"""Exercise the catalog merger at its command-line entry point."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


EXODUS = Path(__file__).resolve().parents[1] / "exodus"


def catalog_entry(provider, provider_id, name):
    return {
        "provider": provider,
        "provider_id": provider_id,
        "name": name,
        "artwork": None,
        "installation_state": "unknown",
        "last_played": None,
        "compatibility_state": "unknown",
        "launch_mode": None,
    }


class CatalogMergeTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)

    def write_catalog(self, name, entries):
        path = self.root / name
        path.write_text(json.dumps({"schema_version": 1, "entries": entries}, ensure_ascii=False),
                        encoding="utf-8")
        return path

    def run_command(self, script, *paths):
        return subprocess.run(
            [sys.executable, "-X", "utf8", str(EXODUS / script), *map(str, paths)],
            capture_output=True, encoding="utf-8",
        )

    def test_merges_a_switch_snapshot_with_the_steam_importer_output(self):
        switch_entry = catalog_entry("switch", "0100abcd12340000", '遊び "A" \\ B\n')
        switch_catalog = self.write_catalog("switch.json", [switch_entry])
        steam_directory = self.root / "steamapps"
        steam_directory.mkdir()
        for app_id in (12, 2):
            manifest = steam_directory / f"appmanifest_{app_id}.acf"
            manifest.write_text(f'AppState {{ appid {app_id} name Game StateFlags 4 }}',
                                encoding="utf-8")
        steam_import = self.run_command("steam_catalog.py", self.root)
        self.assertEqual((steam_import.returncode, steam_import.stderr), (0, ""))
        steam_catalog = self.root / "steam.json"
        steam_catalog.write_text(steam_import.stdout, encoding="utf-8")

        result = self.run_command("catalog_merge.py", switch_catalog, steam_catalog)
        self.assertEqual((result.returncode, result.stderr), (0, ""))
        steam_entries = json.loads(steam_import.stdout)["entries"]
        self.assertEqual([entry["provider_id"] for entry in steam_entries], ["2", "12"])
        self.assertEqual(json.loads(result.stdout),
                         {"schema_version": 1, "entries": [switch_entry, *steam_entries]})

    def test_identity_includes_provider_and_identical_entries_deduplicate(self):
        first = catalog_entry("switch", "0100000000000000", "Native entry")
        second = catalog_entry("other_provider", first["provider_id"], "Independent entry")
        result = self.run_command("catalog_merge.py",
                                  self.write_catalog("first.json", [first]),
                                  self.write_catalog("second.json", [first, second]))
        self.assertEqual((result.returncode, result.stderr), (0, ""))
        self.assertEqual(json.loads(result.stdout)["entries"], [first, second])

    def test_conflicting_identity_fails_before_writing_the_catalog(self):
        first = catalog_entry("steam", "12", "Game")
        second = {**first, "installation_state": "reported_installed"}
        result = self.run_command("catalog_merge.py",
                                  self.write_catalog("first.json", [first]),
                                  self.write_catalog("second.json", [second]))
        self.assertEqual((result.returncode, result.stdout), (1, ""))
        self.assertIn("conflicting entries", result.stderr)

    def test_rejects_bad_catalogs_without_emitting_partial_data(self):
        entry = catalog_entry("steam", "12", "Ready")
        good_path = self.write_catalog("good.json", [entry])
        bad_path = self.root / "bad.json"
        invalid_catalogs = (
            b'{"schema_version":1,"schema_version":1,"entries":[]}',
            b'{"schema_version":true,"entries":[]}',
            b'{"schema_version":2,"entries":[]}',
            b'{"schema_version":1,"entries":[\xff]}',
            b'[' * 3000 + b']' * 3000,
            json.dumps({"schema_version": 1, "entries": [{**entry, "last_played": True}]}).encode(),
            json.dumps({"schema_version": 1, "entries": [{**entry, "name": "\ud800"}]}).encode(),
            json.dumps({"schema_version": 1, "entries": [{**entry, "account_id": "x"}]}).encode(),
        )
        for contents in invalid_catalogs:
            with self.subTest(contents=contents[:60]):
                bad_path.write_bytes(contents)
                result = self.run_command("catalog_merge.py", good_path, bad_path)
                self.assertEqual((result.returncode, result.stdout), (1, ""))
                self.assertIn("bad.json", result.stderr)
                self.assertNotIn("Traceback", result.stderr)

    def test_rejects_oversize_and_nonfile_inputs(self):
        large_path = self.root / "large.json"
        with large_path.open("wb") as stream:
            stream.truncate(16 * 1024 * 1024 + 1)
        for path, reason in ((large_path, "catalog exceeds 16 MiB"),
                             (self.root, "expected a regular catalog file")):
            with self.subTest(path=path):
                result = self.run_command("catalog_merge.py", path)
                self.assertEqual((result.returncode, result.stdout), (1, ""))
                self.assertIn(reason, result.stderr)


if __name__ == "__main__":
    unittest.main()

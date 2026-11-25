"""Exercise the user-facing Steam manifest importer in isolated libraries."""

import json
import os
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

    def scan(self, *paths, cwd=None):
        result = subprocess.run(
            [sys.executable, "-X", "utf8", str(SCRIPT), *map(str, paths)],
            cwd=cwd, capture_output=True, text=True, encoding="utf-8",
        )
        return result.returncode, json.loads(result.stdout), result.stderr

    def manifest(self, directory, app_id, name, flags=None):
        fields = f'"appid" "{app_id}"\n"name" "{name}"'
        if flags is not None:
            fields += f'\n"StateFlags" "{flags}"'
        (directory / f"appmanifest_{app_id}.acf").write_text(
            f'"AppState"\n{{\n{fields}\n}}\n', encoding="utf-8"
        )

    @staticmethod
    def quoted(value):
        return '"' + str(value).replace("\\", "\\\\").replace('"', '\\"') + '"'

    def library_config(self, text, *, installation=None, fallback_location=False):
        installation = installation or self.first.parent
        folder = installation / ("config" if fallback_location else "steamapps")
        folder.mkdir(parents=True, exist_ok=True)
        path = folder / "libraryfolders.vdf"
        path.write_text(text, encoding="utf-8-sig")
        return path

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

    def test_duplicate_keys_preserve_first_match_in_both_catalog_commands(self):
        (self.first / "appmanifest_42.acf").write_text(
            '''
            AppState {
                appid 42 Name "First title" name "Stale title"
                StateFlags 4 stateflags 2 LastPlayed 1700000000 lastplayed 0
            }
            APPSTATE { appid 42 name "Stale block" StateFlags 2 LastPlayed 0 }
            ''', encoding="utf-8",
        )
        (self.first / "appmanifest_43.acf").write_text(
            'AppState { appid 99 APPID 43 name "Mismatched app" }', encoding="utf-8",
        )
        self.manifest(self.first, 44, "Readable neighbor", 4)
        (self.first / "appmanifest_45.acf").write_text(
            'AppState { appid 45 name First InstalledDepots {} installeddepots { file "unfinished',
            encoding="utf-8",
        )

        direct = self.scan(self.first)
        combined = subprocess.run(
            [sys.executable, "-B", "-X", "utf8", "-m", "exodus.app_catalog", "--steam-library", str(self.first)],
            cwd=SCRIPT.parent.parent, capture_output=True, text=True, encoding="utf-8",
        )
        self.assertEqual((combined.returncode, json.loads(combined.stdout), combined.stderr), direct)
        code, output, errors = direct
        self.assertEqual(code, 1)
        self.assertEqual(
            [(entry["provider_id"], entry["name"], entry["installation_state"], entry["last_played"])
             for entry in output["entries"]],
            [("42", "First title", "reported_installed", 1700000000),
             ("44", "Readable neighbor", "reported_installed", None)],
        )
        self.assertIn("appmanifest_43.acf: appid must be a positive Steam App ID matching the filename", errors)
        self.assertIn("appmanifest_45.acf: unterminated quoted string", errors)

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

    def test_dot_and_parent_paths_select_steamapps_in_both_commands(self):
        common = self.first / "common"
        common.mkdir()
        self.manifest(self.first, 42, "Recorded title", 4)
        expected = self.scan(self.first)
        self.assertEqual((expected[0], expected[2]), (0, ""))
        self.assertEqual([entry["provider_id"] for entry in expected[1]["entries"]], ["42"])
        for cwd, source in ((self.first, "."), (common, ".."), (self.first.parent, ".")):
            with self.subTest(cwd=cwd, source=source):
                self.assertEqual(self.scan(source, cwd=cwd), expected)

        for source in (self.first, common / ".."):
            with self.subTest(combined=source):
                result = subprocess.run(
                    [sys.executable, "-B", "-X", "utf8", "-m", "exodus.app_catalog",
                     "--steam-library", str(source)],
                    cwd=SCRIPT.parent.parent, capture_output=True, text=True, encoding="utf-8",
                )
                self.assertEqual((result.returncode, json.loads(result.stdout), result.stderr), expected)

    def test_steamapps_parent_path_respects_intermediary_access(self):
        self.manifest(self.first, 42, "Recorded title", 4)
        regular_file = self.first / "regular-file.txt"
        regular_file.write_text("sample", encoding="utf-8")
        expected = self.scan(self.first)
        for intermediary in (self.first / "missing", regular_file):
            source = intermediary / ".."
            accessible = source.is_dir()
            for libraries in ((source,), (self.first, source), (source, self.first)):
                with self.subTest(libraries=libraries):
                    result = self.scan(*libraries)
                    if accessible:
                        self.assertEqual(result, expected)
                    else:
                        code, catalog, errors = result
                        entries = expected[1]["entries"] if self.first in libraries else []
                        self.assertEqual((code, catalog["entries"]), (1, entries))
                        self.assertIn(str(source), errors)

    def test_root_named_steamapps_imports_nested_manifests_in_both_commands(self):
        library = self.first.parent / "SteamApps"
        nested = library / "steamapps"
        nested.mkdir(parents=True)
        self.manifest(nested, 42, "Nested library", 4)

        expected = self.scan(nested)
        self.assertEqual((expected[0], expected[2]), (0, ""))
        self.assertEqual([entry["provider_id"] for entry in expected[1]["entries"]], ["42"])
        self.assertEqual(self.scan(library), expected)

        combined = subprocess.run(
            [sys.executable, "-B", "-X", "utf8", "-m", "exodus.app_catalog", "--steam-library", str(library)],
            cwd=SCRIPT.parent.parent, capture_output=True, text=True, encoding="utf-8",
        )
        self.assertEqual((combined.returncode, json.loads(combined.stdout), combined.stderr), expected)

    def test_overlapping_sources_keep_nested_library_in_both_commands(self):
        nested = self.first / "steamapps"
        nested.mkdir()
        self.manifest(nested, 42, "Nested library", 4)
        self.manifest(self.second, 10, "Default library", 4)
        self.library_config(
            f'LibraryFolders {{ 1 {{ path {self.quoted(self.first)} }} }}',
            installation=self.second.parent,
        )
        cases = [
            ((self.first.parent, self.first), (), ["42"]),
            ((self.first, self.first.parent), (), ["42"]),
            ((self.first.parent,), ("--steam-install", self.second.parent), ["10", "42"]),
        ]
        for libraries, discovery, ids in cases:
            with self.subTest(libraries=libraries, discovery=discovery):
                direct = self.scan(*libraries, *discovery)
                self.assertEqual((direct[0], direct[2]), (0, ""))
                self.assertEqual([entry["provider_id"] for entry in direct[1]["entries"]], ids)

                combined_args = []
                for library in libraries:
                    combined_args.extend(("--steam-library", str(library)))
                combined = subprocess.run(
                    [sys.executable, "-B", "-X", "utf8", "-m", "exodus.app_catalog",
                     *combined_args, *map(str, discovery)],
                    cwd=SCRIPT.parent.parent, capture_output=True, text=True, encoding="utf-8",
                )
                self.assertEqual((combined.returncode, json.loads(combined.stdout), combined.stderr), direct)

    def test_direct_manifest_candidates_keep_precedence_over_nested_directory(self):
        nested = self.first / "steamapps"
        nested.mkdir()
        self.manifest(nested, 42, "Nested library", 4)
        (self.first / "appmanifest_7.acf").write_text(
            'AppState { appid 7 name "unfinished', encoding="utf-8",
        )

        code, catalog, errors = self.scan(self.first)
        self.assertEqual((code, catalog["entries"]), (1, []))
        self.assertIn("appmanifest_7.acf: unterminated quoted string", errors)

        self.manifest(self.first, 8, "Direct library", 4)
        code, catalog, errors = self.scan(self.first)
        self.assertEqual((code, [entry["provider_id"] for entry in catalog["entries"]]), (1, ["8"]))
        self.assertIn("appmanifest_7.acf: unterminated quoted string", errors)

    def test_discovers_modern_libraries_without_exporting_configuration_metadata(self):
        self.manifest(self.first, 42, "Default game", 4)
        self.manifest(self.second, 43, "Extra game", 4)
        self.library_config(
            '"LibraryFolders" { contentstatsid "private-account-marker" '
            f'"2" {{ "path" {self.quoted(self.second.parent)} '
            '"apps" { "99" "1234" } "label" "Private label" } '
            f'"0" {{ "path" {self.quoted(self.first.parent)} }} }}'
        )
        # Steam's primary location takes precedence over this valid stale copy.
        self.library_config(
            f'LibraryFolders {{ 0 {self.quoted(self.first.parent)} }}', fallback_location=True,
        )

        code, output, errors = self.scan("--steam-install", self.first.parent)
        self.assertEqual((code, errors), (0, ""))
        self.assertEqual([entry["provider_id"] for entry in output["entries"]], ["42", "43"])
        encoded = json.dumps(output)
        for private in (str(self.first.parent), "private-account-marker", "Private label"):
            self.assertNotIn(private, encoded)

    def test_legacy_and_modern_paths_work_in_both_catalog_commands(self):
        self.manifest(self.first, 42, "Default game", 4)
        self.manifest(self.second, 43, "Additional game", 4)
        self.library_config(
            'LibraryFolders { TimeNextStatsReport 123 '
            f'1 {self.quoted(self.second.parent)} '
            f'2 {{ path {self.quoted(self.first.parent)} }} }}',
            fallback_location=True,
        )
        expected = self.scan("--steam-install", self.first.parent)
        self.assertEqual((expected[0], expected[2]), (0, ""))
        self.assertEqual([entry["provider_id"] for entry in expected[1]["entries"]], ["42", "43"])

        combined = subprocess.run(
            [sys.executable, "-B", "-X", "utf8", "-m", "exodus.app_catalog",
             "--steam-install", str(self.first.parent)],
            cwd=SCRIPT.parent.parent, capture_output=True, text=True, encoding="utf-8",
        )
        self.assertEqual((combined.returncode, json.loads(combined.stdout), combined.stderr), expected)

    def test_discovery_keeps_readable_entries_when_some_library_paths_are_invalid(self):
        self.manifest(self.first, 42, "Default game", 4)
        self.manifest(self.second, 43, "Additional game", 4)
        foreign = "/foreign/steam" if os.name == "nt" else r"C:\ForeignSteam"
        self.library_config(
            'LibraryFolders { 0 "" 1 "relative/library" 2 { path {} } '
            f'3 {self.quoted(foreign)} 4 {self.quoted(chr(0))} '
            f'5 {{ apps {{ 42 1 }} }} 6 {self.quoted(self.second.parent)} }}'
        )

        code, output, errors = self.scan("--steam-install", self.first.parent)
        self.assertEqual(code, 1)
        self.assertEqual([entry["provider_id"] for entry in output["entries"]], ["42", "43"])
        self.assertEqual(errors.count("requires an absolute path for this host"), 6)
        self.assertNotIn("Traceback", errors)

    def test_missing_corrupt_and_oversized_configuration_preserve_default_library(self):
        self.manifest(self.first, 42, "Default game", 4)
        configuration = self.first / "libraryfolders.vdf"
        invalid = [None, b"", b'Other {}', b'LibraryFolders { 1 "unfinished',
                   b'LibraryFolders { 1 "\xff" }', b" " * (16 * 1024 * 1024 + 1)]
        for contents in invalid:
            with self.subTest(contents=None if contents is None else contents[:50]):
                if contents is not None:
                    configuration.write_bytes(contents)
                code, output, errors = self.scan("--steam-install", self.first.parent)
                self.assertEqual(code, 1)
                self.assertEqual([entry["provider_id"] for entry in output["entries"]], ["42"])
                self.assertIn("libraryfolders.vdf", errors)
                self.assertNotIn("Traceback", errors)
        configuration.unlink()
        configuration.mkdir()
        code, output, errors = self.scan("--steam-install", self.first.parent)
        self.assertEqual((code, len(output["entries"])), (1, 1))
        self.assertIn("expected a regular library configuration file", errors)

    def test_invalid_preferred_configuration_does_not_fall_back_to_stale_libraries(self):
        self.manifest(self.first, 42, "Default game", 4)
        self.manifest(self.second, 43, "Stale game", 4)
        self.library_config('LibraryFolders { 0 "unfinished')
        self.library_config(
            f'LibraryFolders {{ 1 {self.quoted(self.second.parent)} }}', fallback_location=True,
        )
        code, output, errors = self.scan("--steam-install", self.first.parent)
        self.assertEqual(code, 1)
        self.assertEqual([entry["provider_id"] for entry in output["entries"]], ["42"])
        self.assertIn("unterminated quoted string", errors)

    def test_explicit_libraries_precede_discovery_and_installed_copies_still_win(self):
        self.manifest(self.first, 42, "Default copy", 4)
        self.manifest(self.second, 42, "Explicit copy", 4)
        self.manifest(self.first, 43, "Installed copy", 4)
        self.manifest(self.second, 43, "Updating copy", 1024)
        self.library_config("LibraryFolders {}")
        code, output, errors = self.scan(
            self.second, "--steam-install", self.first.parent, self.first,
        )
        self.assertEqual((code, errors), (0, ""))
        self.assertEqual([entry["name"] for entry in output["entries"]],
                         ["Explicit copy", "Installed copy"])

    def test_numeric_library_order_and_multiple_installations_are_deterministic(self):
        third = self.first.parent.parent / "Third library Игры" / "steamapps"
        third.mkdir(parents=True)
        self.manifest(self.second, 42, "Second library", 4)
        self.manifest(third, 42, "Third library", 4)
        self.manifest(third, 43, "Other game", 4)
        self.library_config(
            f'LibraryFolders {{ 10 {self.quoted(third.parent)} '
            f'2 {{ path {self.quoted(self.second.parent)} }} }}'
        )
        self.library_config("LibraryFolders {}", installation=third.parent)
        code, output, errors = self.scan(
            "--steam-install", self.first.parent, "--steam-install", third.parent,
        )
        self.assertEqual((code, errors), (0, ""))
        self.assertEqual([entry["name"] for entry in output["entries"]],
                         ["Second library", "Other game"])

    def test_duplicate_roots_and_steamapps_are_scanned_once(self):
        (self.first / "appmanifest_42.acf").write_text('AppState {', encoding="utf-8")
        self.library_config(
            f'LibraryFolders {{ 0 {self.quoted(self.first.parent)} '
            f'1 {{ path {self.quoted(self.first)} }} }}'
        )
        code, output, errors = self.scan(
            self.first, self.first.parent, "--steam-install", self.first.parent,
            "--steam-install", self.first.parent,
        )
        self.assertEqual((code, output["entries"]), (1, []))
        self.assertEqual(errors.count("appmanifest_42.acf"), 1)

    def test_symlink_library_alias_is_scanned_once(self):
        alias = self.first.parent.parent / "Alias"
        try:
            alias.symlink_to(self.first.parent, target_is_directory=True)
        except (OSError, NotImplementedError):
            self.skipTest("directory symlinks are not available")
        (self.first / "appmanifest_42.acf").write_text('AppState {', encoding="utf-8")
        self.library_config(f'LibraryFolders {{ 0 {self.quoted(alias)} }}')
        code, output, errors = self.scan(
            alias, "--steam-install", self.first.parent, "--steam-install", alias,
        )
        self.assertEqual((code, output["entries"]), (1, []))
        self.assertEqual(errors.count("appmanifest_42.acf"), 1)

    def test_missing_discovered_library_keeps_the_default_and_other_libraries(self):
        self.manifest(self.first, 42, "Default game", 4)
        self.manifest(self.second, 43, "Additional game", 4)
        self.library_config(
            f'LibraryFolders {{ 1 {self.quoted(self.first.parent / "unmounted")} '
            f'2 {self.quoted(self.second.parent)} }}'
        )
        code, output, errors = self.scan("--steam-install", self.first.parent)
        self.assertEqual(code, 1)
        self.assertEqual([entry["provider_id"] for entry in output["entries"]], ["42", "43"])
        self.assertIn("unmounted", errors)

    def test_steam_install_is_a_root_even_when_named_steamapps(self):
        installation = self.first.parent / "SteamApps"
        manifests = installation / "steamapps"
        manifests.mkdir(parents=True)
        self.manifest(manifests, 42, "Nested default library", 4)
        self.library_config("LibraryFolders {}", installation=installation)
        code, output, errors = self.scan("--steam-install", installation)
        self.assertEqual((code, errors), (0, ""))
        self.assertEqual([entry["provider_id"] for entry in output["entries"]], ["42"])

    def test_no_library_source_is_a_usage_error(self):
        result = subprocess.run([sys.executable, str(SCRIPT)], capture_output=True, text=True)
        self.assertEqual((result.returncode, result.stdout), (2, ""))
        self.assertIn("supply at least one", result.stderr)


if __name__ == "__main__":
    unittest.main()

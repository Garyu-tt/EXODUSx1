"""Keep offline exports and atomic Steam refreshes within HOME's input limits."""

from io import BytesIO, StringIO, TextIOWrapper
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

from exodus import steam_catalog as catalog
from exodus import steam_sync as sync


REPO = Path(__file__).resolve().parents[1]
BYTE_LIMIT = 2 * 1024 * 1024
ENTRY_LIMIT = 4096


def reference_bytes(entries):
    return (json.dumps({"schema_version": 1, "entries": entries},
                       ensure_ascii=True, indent=2) + "\n").encode("ascii")


def byte_boundary_entries(extra=0):
    entry = catalog.steam_entry("42", "界🚀", "reported_installed")
    entry["name"] += "a" * (BYTE_LIMIT + extra - len(reference_bytes([entry])))
    return [entry]


class CatalogOutputLimitTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)
        self.steamapps = self.root / "Steam" / "steamapps"
        self.steamapps.mkdir(parents=True)

    def manifest(self, app_id, name):
        (self.steamapps / f"appmanifest_{app_id}.acf").write_text(
            f'AppState {{ appid {app_id} name "{name}" StateFlags 4 }}', encoding="utf-8",
        )

    def command(self, combined=False, *extra):
        args = (["-m", "exodus.app_catalog", "--steam-library"] if combined else
                [str(REPO / "exodus" / "steam_catalog.py")])
        return subprocess.run(
            [sys.executable, "-B", "-X", "utf8", *args, str(self.steamapps), *map(str, extra)],
            cwd=REPO, capture_output=True, check=False,
        )

    def assert_limit_error(self, result, kind):
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, b"")
        self.assertIn(f"{kind} limit".encode("ascii"), result.stderr)
        self.assertNotIn(b"Traceback", result.stderr)

    def test_entry_boundary_in_both_commands_and_combined_steam_homebrew(self):
        for app_id in range(1, ENTRY_LIMIT + 2):
            self.manifest(app_id, f"App {app_id}")
        for combined in (False, True):
            with self.subTest(combined=combined, entries=ENTRY_LIMIT + 1):
                self.assert_limit_error(self.command(combined), "entry")

        (self.steamapps / f"appmanifest_{ENTRY_LIMIT + 1}.acf").unlink()
        for combined in (False, True):
            with self.subTest(combined=combined, entries=ENTRY_LIMIT):
                result = self.command(combined)
                self.assertEqual((result.returncode, result.stderr), (0, b""))
                self.assertEqual(len(json.loads(result.stdout)["entries"]), ENTRY_LIMIT)
                self.assertLessEqual(len(result.stdout), BYTE_LIMIT)

        switch = self.root / "SD" / "switch"
        switch.mkdir(parents=True)
        nro = bytearray(0x100)
        struct.pack_into("<4sII", nro, 0x10, b"NRO0", 0, len(nro))
        (switch / "Tools.nro").write_bytes(nro)
        self.assert_limit_error(self.command(True, "--switch-sd", switch.parent), "entry")

    def test_exact_byte_limit_and_one_byte_over_in_both_commands(self):
        for extra in (0, 1):
            entries = byte_boundary_entries(extra)
            self.manifest(42, entries[0]["name"])
            for combined in (False, True):
                with self.subTest(combined=combined, extra=extra):
                    result = self.command(combined)
                    if extra:
                        self.assert_limit_error(result, "size")
                    else:
                        self.assertEqual((result.returncode, result.stderr), (0, b""))
                        self.assertEqual(len(result.stdout), BYTE_LIMIT)
                        self.assertEqual(result.stdout, reference_bytes(entries))

    def test_unicode_escape_expansion_counts_toward_the_wire_limit(self):
        name = "界" * 360000
        self.assertLess(len(name.encode("utf-8")), BYTE_LIMIT)
        self.manifest(42, name)
        for combined in (False, True):
            with self.subTest(combined=combined):
                self.assert_limit_error(self.command(combined), "size")

    def test_standalone_file_output_respects_exact_byte_limit(self):
        output = self.root / "selected catalog.json"
        output.write_bytes(b"previous snapshot")
        entries = byte_boundary_entries()
        self.manifest(42, entries[0]["name"])
        result = self.command(False, "--output", output)
        self.assertEqual((result.returncode, result.stdout, result.stderr), (0, b"", b""))
        self.assertEqual(output.stat().st_size, BYTE_LIMIT)
        self.assertEqual(output.read_bytes(), reference_bytes(entries))

        self.manifest(42, byte_boundary_entries(1)[0]["name"])
        self.assert_limit_error(self.command(False, "--output", output), "size")
        self.assertEqual(output.read_bytes(), reference_bytes(entries))

    def test_standalone_file_publish_failure_cleans_stage(self):
        self.manifest(42, "Readable game")
        output = self.root / "catalog.json"
        output.mkdir()
        result = self.command(False, "--output", output)
        self.assertEqual((result.returncode, result.stdout), (1, b""))
        self.assertIn(b"Could not write the catalog. The previous output was preserved.", result.stderr)
        self.assertTrue(output.is_dir())
        self.assertEqual(list(self.root.glob(".exodus-catalog-*")), [])

    def test_windows_stdout_translation_cannot_expand_an_exact_limit_catalog(self):
        entries = byte_boundary_entries()
        binary = BytesIO()
        output = TextIOWrapper(binary, encoding="utf-8", newline="\r\n")
        self.addCleanup(output.close)
        with patch.object(sys, "stdout", output):
            catalog.write_catalog(entries)
        output.flush()
        self.assertEqual(binary.getvalue(), reference_bytes(entries))
        self.assertEqual(len(binary.getvalue()), BYTE_LIMIT)
        self.assertNotIn(b"\r\n", binary.getvalue())

    def test_writer_checks_both_limits_before_writing_any_text(self):
        for entries, kind in (
            ([catalog.steam_entry(str(index + 1), "App") for index in range(ENTRY_LIMIT + 1)], "entry"),
            (byte_boundary_entries(1), "size"),
        ):
            with self.subTest(kind=kind):
                output = StringIO()
                with self.assertRaisesRegex(ValueError, f"{kind} limit"):
                    catalog.write_catalog(entries, output)
                self.assertEqual(output.getvalue(), "")

    def test_atomic_refresh_rejects_both_limits_before_staging(self):
        output = self.root / "catalog.json"
        previous = reference_bytes([catalog.steam_entry("7", "Previous snapshot")])
        output.write_bytes(previous)
        for entries, kind in (
            ([catalog.steam_entry(str(index + 1), "App") for index in range(ENTRY_LIMIT + 1)], "entry"),
            (byte_boundary_entries(1), "size"),
        ):
            with self.subTest(kind=kind), patch.object(catalog, "NamedTemporaryFile") as stage, \
                    patch.object(catalog.os, "replace") as replace:
                with self.assertRaisesRegex(sync.SteamSyncError, f"{kind} limit"):
                    sync.save_catalog(output, entries)
                stage.assert_not_called()
                replace.assert_not_called()
                self.assertEqual(output.read_bytes(), previous)

    def test_atomic_refresh_accepts_the_exact_byte_limit(self):
        output = self.root / "catalog.json"
        entries = byte_boundary_entries()
        sync.save_catalog(output, entries)
        self.assertEqual(output.read_bytes(), reference_bytes(entries))
        self.assertEqual(output.stat().st_size, BYTE_LIMIT)

    def test_readable_partial_scans_still_emit_catalogs_with_exit_one(self):
        self.manifest(42, "Readable game")
        (self.steamapps / "appmanifest_43.acf").write_text('AppState {', encoding="utf-8")
        for combined in (False, True):
            with self.subTest(combined=combined):
                result = self.command(combined)
                self.assertEqual(result.returncode, 1)
                self.assertEqual([entry["provider_id"] for entry in json.loads(result.stdout)["entries"]],
                                 ["42"])
                self.assertIn(b"appmanifest_43.acf", result.stderr)

        output = self.root / "partial catalog.json"
        result = self.command(False, "--output", output)
        self.assertEqual((result.returncode, result.stdout), (1, b""))
        self.assertEqual([entry["provider_id"] for entry in json.loads(output.read_bytes())["entries"]], ["42"])
        self.assertIn(b"appmanifest_43.acf", result.stderr)


if __name__ == "__main__":
    unittest.main()

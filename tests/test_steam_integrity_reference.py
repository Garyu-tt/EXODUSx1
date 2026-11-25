"""Exercise trusted-library reference export without a Steam client."""

import hashlib
import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from exodus.steam_integrity import reference


REPO_ROOT = Path(__file__).resolve().parents[1]
EXPANSION_PAIRS = (
    ("Straße.dat", "STRASSE.dat"),
    ("ﬀ.dat", "ff.dat"),
    ("İ.dat", "i\u0307.dat"),
    ("ßs.dat", "sß.dat"),
)


class SteamIntegrityReferenceTests(unittest.TestCase):
    def setUp(self):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        self.root = Path(temp.name)
        self.steamapps = self.root / "steamapps"
        self.game = self.steamapps / "common" / "Sample Game"
        self.game.mkdir(parents=True)
        self.output = self.root / "42.files"
        self.manifest()

    def manifest(self, filename_id="42", app_id="42", directory="Sample Game", flags="4"):
        fields = f'"appid" "{app_id}" "installdir" "{directory}" "LastOwner" "private-id"'
        if flags is not None:
            fields += f' "StateFlags" "{flags}"'
        (self.steamapps / f"appmanifest_{filename_id}.acf").write_text(
            f"AppState {{ {fields} }}\n", encoding="utf-8-sig",
        )

    def file(self, relative, contents=b"abc"):
        path = self.game / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(contents)
        return path

    def export(self, output=None, app_id="42"):
        return reference.export_reference(self.steamapps, app_id, output or self.output)

    def test_exact_bytes_are_sorted_and_app_bound(self):
        self.manifest(flags="6")
        contents = [
            ("A Folder/Part one.txt", b"hello\r\nworld\x00"),
            ("b empty.bin", b""),
            ("Launch.exe", b"sample file content"),
            ("Z.log", b"abc"),
            ("Ж/É.dat", b"utf8 pathname"),
        ]
        for relative, data in reversed(contents):
            self.file(relative, data)
        self.manifest(filename_id="43", app_id="43", directory="Unrelated")
        expected = b"EXODUS-STEAM-SHA256 1\n42\n5\n" + b"".join(
            f"{hashlib.sha256(data).hexdigest()}\t{len(data)}\t{relative}\n".encode("utf-8")
            for relative, data in contents
        )
        with patch.object(reference, "READ_BYTES", 3):
            pin = self.export()
        self.assertEqual(self.output.read_bytes(), expected)
        self.assertEqual(pin, hashlib.sha256(expected).hexdigest())
        for relative, data in contents:
            self.assertEqual((self.game / relative).read_bytes(), data)

    def test_cli_prints_only_the_reference_digest(self):
        self.file("steam sample.txt")
        command = [sys.executable, "-B", "-X", "utf8", "-m", "exodus.steam_integrity.reference",
                   "--steamapps", str(self.steamapps), "--app-id", "42", "--output", str(self.output)]
        run = subprocess.run(command, cwd=REPO_ROOT, capture_output=True, text=True, encoding="utf-8")
        self.assertEqual((run.returncode, run.stderr), (0, ""))
        self.assertEqual(run.stdout, hashlib.sha256(self.output.read_bytes()).hexdigest() + "\n")
        duplicate = subprocess.run(command, cwd=REPO_ROOT, capture_output=True, text=True, encoding="utf-8")
        self.assertEqual((duplicate.returncode, duplicate.stdout), (1, ""))
        self.assertIn("already exists", duplicate.stderr)

    def test_exact_appid_validation(self):
        self.file("game.bin")
        for value in ("0", "042", "+42", "42\n", "４２", "4294967296"):
            with self.subTest(value=value), self.assertRaisesRegex(ValueError, "canonical positive uint32"):
                self.export(app_id=value)
        self.assertFalse(self.output.exists())
        self.manifest(filename_id="4294967295", app_id="4294967295")
        self.export(app_id="4294967295")
        self.assertTrue(self.output.read_bytes().startswith(b"EXODUS-STEAM-SHA256 1\n4294967295\n1\n"))

    def test_stale_or_unsafe_installation_leaves_output_absent(self):
        self.file("game.bin")
        for app_id, directory, flags, error in (
            ("43", "Sample Game", "4", "App ID does not match"),
            ("042", "Sample Game", "4", "App ID does not match"),
            ("42", "Sample Game", "2", "fully installed"),
            ("42", "Sample Game", None, "fully installed"),
            ("42", "Sample Game", "4294967296", "invalid stateflags"),
            ("42", "../Sample Game", "4", "unsafe path component"),
            ("42", "NUL.txt", "4", "reserved path component"),
        ):
            with self.subTest(app_id=app_id, directory=directory, flags=flags):
                self.manifest(app_id=app_id, directory=directory, flags=flags)
                with self.assertRaisesRegex(ValueError, error):
                    self.export()
                self.assertFalse(self.output.exists())
        self.manifest(directory="Missing game")
        with self.assertRaises(FileNotFoundError):
            self.export()
        self.assertFalse(self.output.exists())

    def test_rejects_unsafe_record_components(self):
        for name in ("", ".", "..", "a/b", "a\\b", "a:b", "a\tb", "a\x00b", "a\x7fb", "a\u009fb",
                     " a", "a ", "a.", 'a"b', "x<y", "x>y", "x|y", "x*y", "x?y", "lpt1.dat",
                     "con .ini", "cOm¹.bin", "a" * 256, "É" * 128):
            with self.subTest(name=name), self.assertRaises(ValueError):
                reference._component_bytes(name)

    def test_refuses_empty_game_or_output_inside_it(self):
        with self.assertRaisesRegex(ValueError, "no regular files"):
            self.export()
        self.file("data/game.bin")
        for inside in (self.game / "42.files", self.game / "data" / "42.files"):
            with self.subTest(inside=inside), self.assertRaisesRegex(ValueError, "outside the game directory"):
                self.export(output=inside)
            self.assertFalse(inside.exists())
        self.assertFalse(self.output.exists())

    def test_rejects_case_aliased_directories(self):
        self.file("Other/a.txt")
        self.file("other/b.txt")
        if len(list(self.game.iterdir())) != 2:
            self.skipTest("case-distinct directory names unavailable on this filesystem")
        with self.assertRaisesRegex(ValueError, "case-alias"):
            self.export()
        self.assertFalse(self.output.exists())

    def assert_exported_contents(self, contents, output=None):
        output = output or self.output
        paths = sorted(contents, key=lambda name: name.encode("utf-8").lower())
        expected = f"EXODUS-STEAM-SHA256 1\n42\n{len(paths)}\n".encode("ascii")
        for name in paths:
            data = contents[name]
            expected += (f"{hashlib.sha256(data).hexdigest()}\t{len(data)}\t{name}\n"
                         .encode("utf-8"))
        self.assertEqual(output.read_bytes(), expected)
        for name, data in contents.items():
            self.assertEqual((self.game / name).read_bytes(), data)
        return hashlib.sha256(expected).hexdigest()

    def test_unicode_expansion_file_and_directory_names_remain_distinct(self):
        contents = {"a-first.dat": b"lowercase ASCII", "Z-last-ascii.dat": b"uppercase ASCII"}
        for number, pair in enumerate(EXPANSION_PAIRS):
            for name in pair:
                for relative in (f"files-{number}/{name}", f"directories-{number}/{name}/part.bin"):
                    contents[relative] = ("payload for " + relative).encode("utf-8")
        # Create in reverse order so traversal order cannot determine the wire order.
        for relative, data in reversed(contents.items()):
            self.file(relative, data)
        for number, pair in enumerate(EXPANSION_PAIRS):
            for kind in ("files", "directories"):
                first, second = (self.game / f"{kind}-{number}" / name for name in pair)
                self.assertFalse(first.samefile(second), "Expansion pairs must be separate filesystem entries")

        pin = self.export()
        self.assertEqual(pin, self.assert_exported_contents(contents))

    def test_root_file_directory_expansion_pairs_are_not_aliases(self):
        contents = {}
        for number, pair in enumerate(EXPANSION_PAIRS):
            file_name, directory_name = pair if number % 2 == 0 else reversed(pair)
            contents[file_name] = ("file " + file_name).encode("utf-8")
            contents[directory_name + "/part.bin"] = ("directory " + directory_name).encode("utf-8")
        for relative, data in contents.items():
            self.file(relative, data)
        pin = self.export()
        self.assertEqual(pin, self.assert_exported_contents(contents))

    def test_cli_preserves_unicode_expansion_paths_hashes_and_sources(self):
        contents = {name: ("CLI payload " + name).encode("utf-8")
                    for pair in EXPANSION_PAIRS for name in pair}
        for relative, data in reversed(contents.items()):
            self.file(relative, data)
        manifest = self.steamapps / "appmanifest_42.acf"
        original_manifest = manifest.read_bytes()
        command = [sys.executable, "-B", "-X", "utf8", "-m", "exodus.steam_integrity.reference",
                   "--steamapps", str(self.steamapps), "--app-id", "42"]
        for output in (self.output, self.root / "repeat.files"):
            result = subprocess.run(command + ["--output", str(output)], cwd=REPO_ROOT,
                                    capture_output=True, text=True, encoding="utf-8")
            self.assertEqual((result.returncode, result.stderr), (0, ""))
            pin = self.assert_exported_contents(contents, output)
            self.assertEqual(result.stdout, pin + "\n")
        self.assertEqual(self.output.read_bytes(), (self.root / "repeat.files").read_bytes())
        self.assertEqual(manifest.read_bytes(), original_manifest)

    def test_per_character_aliases_still_reject_files_directories_and_mixed_entries(self):
        aliases = (("Other.dat", "other.dat"), ("Σ.dat", "σ.dat"), ("Straße.dat", "STRAẞE.dat"))
        for pair in aliases:
            for kinds in (("file", "file"), ("directory", "directory"), ("file", "directory")):
                with self.subTest(pair=pair, kinds=kinds), tempfile.TemporaryDirectory(dir=self.root) as temporary:
                    root = Path(temporary)
                    steamapps = root / "steamapps"
                    game = steamapps / "common/Game"
                    game.mkdir(parents=True)
                    (steamapps / "appmanifest_42.acf").write_text(
                        'AppState { appid 42 installdir Game StateFlags 4 }', encoding="utf-8",
                    )
                    first, second = (game / name for name in pair)
                    for path, kind in zip((first, second), kinds):
                        if path.exists():
                            self.skipTest("case aliases already identify one entry on this filesystem")
                        if kind == "directory":
                            path.mkdir()
                            (path / "part.bin").write_bytes(b"directory payload")
                        else:
                            path.write_bytes(b"file payload")
                    self.assertFalse(first.samefile(second))
                    output = root / "42.files"
                    with self.assertRaisesRegex(ValueError, "case-alias"):
                        reference.export_reference(steamapps, "42", output)
                    self.assertFalse(output.exists())

    def test_rejects_links_in_source_and_files(self):
        self.file("real.bin")
        alias = self.game / "alias.bin"
        try:
            alias.symlink_to(self.game / "real.bin")
        except (OSError, NotImplementedError) as exc:
            self.skipTest(f"creating symlinks unavailable: {exc}")
        with self.assertRaisesRegex(ValueError, "links and reparse points"):
            self.export()
        alias.unlink()
        source_link = self.root / "other steamapps"
        source_link.symlink_to(self.steamapps, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "without links or reparse points"):
            reference.export_reference(source_link, "42", self.output)
        actual_manifest = self.steamapps / "appmanifest_42.acf"
        saved_manifest = self.root / "saved.acf"
        actual_manifest.rename(saved_manifest)
        actual_manifest.symlink_to(saved_manifest)
        with self.assertRaisesRegex(ValueError, "without links or reparse points"):
            self.export()
        self.assertFalse(self.output.exists())

    def test_recognizes_windows_reparse_points_without_symlink_mode(self):
        info = SimpleNamespace(st_mode=stat.S_IFDIR, st_file_attributes=stat.FILE_ATTRIBUTE_REPARSE_POINT)
        self.assertTrue(reference._is_link(info))

    @unittest.skipUnless(hasattr(os, "mkfifo"), "FIFO unavailable on this platform")
    def test_rejects_nonregular_game_file(self):
        os.mkfifo(self.game / "pipe")
        with self.assertRaisesRegex(ValueError, "expected a regular game file"):
            self.export()
        self.assertFalse(self.output.exists())

    def test_detects_same_size_change_after_hashing(self):
        path = self.file("game.bin", b"abc")
        original = reference._hash_file

        def hash_then_change(*args):
            digest = original(*args)
            path.write_bytes(b"def")
            info = path.stat()
            os.utime(path, ns=(info.st_atime_ns, info.st_mtime_ns + 2_000_000_000))
            return digest

        with patch.object(reference, "_hash_file", hash_then_change):
            with self.assertRaisesRegex(ValueError, "changed while preparing"):
                self.export()
        self.assertFalse(self.output.exists())

    def test_caps_checked_before_writing(self):
        self.file("a")
        self.file("b")
        exact_size = len(b"EXODUS-STEAM-SHA256 1\n42\n2\n") + 2 * (64 + len(b"\t3\ta\n"))
        for name, limit, error in (
            ("MAX_FILES", 1, "exceeds 1 files"),
            ("MAX_REFERENCE_BYTES", exact_size - 1, "bytes"),
            ("MAX_UINT64", 5, "uint64"),
        ):
            with self.subTest(limit=name), patch.object(reference, name, limit):
                with self.assertRaisesRegex(ValueError, error):
                    self.export()
                self.assertFalse(self.output.exists())
        with patch.object(reference, "MAX_REFERENCE_BYTES", exact_size):
            self.export()
        self.assertEqual(self.output.stat().st_size, exact_size)

    def test_switch_path_limit_counts_full_utf8_path(self):
        if os.name == "nt":
            # Let the fixture reach the Switch limit on a host with legacy MAX_PATH.
            self.steamapps = Path("\\\\?\\" + str(self.steamapps))
            self.game = Path("\\\\?\\" + str(self.game))
        prefix = b"/switch/exodus-steam/steamapps/common/Sample Game/"
        directories = "界" * 85 + "/" + "語" * 85 + "/"
        final_bytes = 768 - len(prefix) - len(directories.encode("utf-8"))
        relative = directories + "調" * (final_bytes // 3) + "x" * (final_bytes % 3)
        self.assertEqual(len(prefix + relative.encode("utf-8")), 768)
        self.file(relative)
        self.export()
        self.assertTrue(self.output.read_bytes().endswith(b"\t" + relative.encode("utf-8") + b"\n"))

        self.file(relative + "x")
        overlong = self.root / "overlong.files"
        with self.assertRaisesRegex(ValueError, "full Switch SD path exceeds 768 UTF-8 bytes"):
            self.export(output=overlong)
        self.assertFalse(overlong.exists())

    def test_acf_size_limit_is_checked_before_parsing(self):
        self.file("game.bin")
        manifest = self.steamapps / "appmanifest_42.acf"
        data = manifest.read_bytes().ljust(64 * 1024, b" ")
        manifest.write_bytes(data)
        self.export()

        manifest.write_bytes(data + b" ")
        oversized = self.root / "oversized-acf.files"
        with patch.object(reference, "parse_keyvalues") as parse:
            with self.assertRaisesRegex(ValueError, "manifest exceeds 64 KiB"):
                self.export(output=oversized)
        parse.assert_not_called()
        self.assertFalse(oversized.exists())

    def test_duplicate_installation_fields_and_appstate_are_rejected(self):
        self.file("game.bin")
        manifest = self.steamapps / "appmanifest_42.acf"
        fields = b' appid 42 installdir "Sample Game" StateFlags 4 '
        for name, append in (("appid", b"APPID 42"), ("installdir", b'InstallDir "Sample Game"'),
                             ("stateflags", b"stateflags 4")):
            with self.subTest(name=name):
                manifest.write_bytes(b"AppState {" + fields + append + b"}")
                with self.assertRaisesRegex(ValueError, f"duplicate {name} key"):
                    self.export()
        manifest.write_bytes(b"AppState {" + fields + b"} APPSTATE {}")
        with self.assertRaisesRegex(ValueError, "duplicate appstate key"):
            self.export()
        manifest.write_bytes(b"AppState {" + fields + b"} other value")
        with self.assertRaisesRegex(ValueError, "expected one AppState object"):
            self.export()
        self.assertFalse(self.output.exists())

    def test_acf_nesting_limit_includes_appstate(self):
        self.file("game.bin")
        manifest = self.steamapps / "appmanifest_42.acf"
        prefix = b'AppState { appid 42 installdir "Sample Game" StateFlags 4 '
        # Repeated irrelevant keys inside metadata preserve generic first-match parsing.
        fields = b" appid 43 APPID 44 "
        manifest.write_bytes(prefix + b"metadata { " * 15 + fields + b"}" * 16)
        self.export()

        manifest.write_bytes(prefix + b"metadata { " * 16 + fields + b"}" * 17)
        nested = self.root / "nested-acf.files"
        with self.assertRaisesRegex(ValueError, "nesting exceeds 16 blocks"):
            self.export(output=nested)
        self.assertFalse(nested.exists())


if __name__ == "__main__":
    unittest.main()

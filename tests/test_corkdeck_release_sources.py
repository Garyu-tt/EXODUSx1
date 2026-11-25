"""Admission tests for the CorkDeck corresponding-source trees."""

import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile
import unittest


REPOSITORY = Path(__file__).resolve().parents[1]
RELEASE_TOOLS = REPOSITORY / "exodus" / "home" / "corkdeck" / "release"
sys.path.insert(0, str(RELEASE_TOOLS))

from dependency_sources import list_files  # noqa: E402


class ReleaseSourceAdmissionTests(unittest.TestCase):
    def setUp(self):
        directory = tempfile.TemporaryDirectory()
        self.addCleanup(directory.cleanup)
        self.root = Path(directory.name)

    def test_regular_tree_is_listed_in_deterministic_relative_order(self):
        source = self.root / "source"
        (source / "z").mkdir(parents=True)
        (source / "a").mkdir()
        (source / "z" / "last.txt").write_text("last", encoding="utf-8")
        (source / "a" / "first.txt").write_text("first", encoding="utf-8")

        files = list_files(source)

        self.assertEqual(list(files), ["a/first.txt", "z/last.txt"])
        self.assertEqual(files["a/first.txt"].read_text(encoding="utf-8"), "first")
        self.assertEqual(files["z/last.txt"].read_text(encoding="utf-8"), "last")

    @unittest.skipUnless(hasattr(os, "symlink"), "symlinks are unavailable")
    def test_directory_symlink_is_rejected_before_descending(self):
        source = self.root / "source"
        outside = self.root / "outside"
        source.mkdir()
        outside.mkdir()
        (outside / "external.txt").write_text("external", encoding="utf-8")
        link = source / "linked"
        try:
            link.symlink_to(outside, target_is_directory=True)
        except OSError as exc:
            self.skipTest(f"directory symlinks unavailable: {exc}")

        with self.assertRaisesRegex(ValueError, "Unsupported file type"):
            list_files(source)

    @unittest.skipUnless(os.name == "nt", "requires Windows directory junctions")
    def test_windows_junctions_cannot_escape_or_replace_the_source_root(self):
        source = self.root / "source"
        outside = self.root / "outside"
        source.mkdir()
        outside.mkdir()
        (outside / "external.txt").write_text("external", encoding="utf-8")

        nested = source / "nested-junction"
        self.create_junction(nested, outside)
        with self.assertRaisesRegex(ValueError, "Unsupported file type"):
            list_files(source)

        root_junction = self.root / "root-junction"
        self.create_junction(root_junction, outside)
        with self.assertRaisesRegex(ValueError, "Expected a directory"):
            list_files(root_junction)

    def create_junction(self, junction, target):
        environment = {
            **os.environ,
            "EXODUS_TEST_JUNCTION_PATH": str(junction),
            "EXODUS_TEST_JUNCTION_TARGET": str(target),
        }
        try:
            created = subprocess.run(
                ["powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
                 "$ErrorActionPreference = 'Stop'; "
                 "New-Item -ItemType Junction -Path $env:EXODUS_TEST_JUNCTION_PATH "
                 "-Target $env:EXODUS_TEST_JUNCTION_TARGET | Out-Null"],
                env=environment, capture_output=True, check=False,
            )
        except FileNotFoundError:
            self.skipTest("Windows PowerShell is unavailable")
        if created.returncode:
            self.skipTest("Windows directory junction unavailable: "
                          + created.stderr.decode(errors="replace"))

        def remove_junction():
            try:
                info = junction.lstat()
            except FileNotFoundError:
                return
            self.assertEqual(info.st_reparse_tag, stat.IO_REPARSE_TAG_MOUNT_POINT)
            self.assertEqual(junction.resolve(strict=True), target.resolve(strict=True))
            junction.rmdir()

        self.addCleanup(remove_junction)
        self.assertEqual(junction.lstat().st_reparse_tag, stat.IO_REPARSE_TAG_MOUNT_POINT)


if __name__ == "__main__":
    unittest.main()

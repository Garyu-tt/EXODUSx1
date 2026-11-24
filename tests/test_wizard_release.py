import os
import unittest
from io import BytesIO
from pathlib import Path
from tempfile import TemporaryDirectory
from zipfile import ZipFile

from exodus.build_wizard_release import PAYLOAD_DIR, WRAPPER_DIR, build_wizard_release


REPO_ROOT = Path(__file__).resolve().parents[1]
VERSION = "26.11.24.07"
PREFIX = "4IFIR Wizard/"
START_INI = "4Advanced/Full Assist/inis/Start.ini"


class WizardReleaseTests(unittest.TestCase):
    def test_archive_contains_the_checked_in_legacy_wizard(self):
        payload_root = REPO_ROOT / PAYLOAD_DIR
        original = {
            path.relative_to(payload_root).as_posix(): path.read_bytes()
            for path in payload_root.rglob("*")
            if path.is_file()
        }
        self.assertIn(START_INI, original)

        with TemporaryDirectory() as directory:
            path = build_wizard_release(REPO_ROOT, Path(directory), VERSION)
            self.assertEqual(path.name, f"4IFIR-Wizard-update-{VERSION}.zip")
            with ZipFile(path) as outer:
                self.assertIsNone(outer.testzip())
                self.assertEqual(
                    outer.namelist(),
                    [PREFIX, PREFIX + "config.ini", PREFIX + "init.ini", PREFIX + "files.zip"],
                )
                for name in ("config.ini", "init.ini"):
                    self.assertEqual(outer.read(PREFIX + name), (REPO_ROOT / WRAPPER_DIR / name).read_bytes())

                with ZipFile(BytesIO(outer.read(PREFIX + "files.zip"))) as payload:
                    self.assertIsNone(payload.testzip())
                    self.assertIn("4Advanced/Full Assist/inis/", payload.namelist())
                    self.assertEqual(
                        sorted(entry.filename for entry in payload.infolist() if not entry.is_dir()),
                        sorted(original),
                    )
                    for name, before in original.items():
                        self.assertEqual((payload_root / name).read_bytes(), before, name)
                        expected = before
                        if name in ("config.ini", START_INI):
                            old_version = next(line for line in before.splitlines() if line.startswith(b";version="))
                            expected = before.replace(old_version, f";version='{VERSION}'".encode(), 1)
                        self.assertEqual(payload.read(name), expected, name)

    def set_up_fixture(self, root):
        payload = root / PAYLOAD_DIR
        version_data = b";version='24.09.29.12'\r\n[Menu]\r\n"
        start_ini = payload / START_INI
        start_ini.parent.mkdir(parents=True)
        start_ini.write_bytes(version_data)
        (payload / "config.ini").write_bytes(version_data)
        wrapper = root / WRAPPER_DIR
        wrapper.mkdir(parents=True)
        (wrapper / "config.ini").write_bytes(b";version='Open to init'")
        (wrapper / "init.ini").write_bytes(b"[Init]")
        return start_ini

    def test_same_inputs_and_version_have_identical_archive_bytes(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            start_ini = self.set_up_fixture(root)
            output = root / "dist"
            archive = build_wizard_release(root, output, VERSION)
            first = archive.read_bytes()
            os.utime(start_ini, (946684800, 946684800))
            self.assertEqual(build_wizard_release(root, output, VERSION).read_bytes(), first)
            with ZipFile(archive) as outer:
                with ZipFile(BytesIO(outer.read(PREFIX + "files.zip"))) as payload:
                    expected = f";version='{VERSION}'\r\n[Menu]\r\n".encode()
                    self.assertEqual(payload.read(START_INI), expected)

    def test_bad_version_markers_do_not_replace_a_previous_build(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            start_ini = self.set_up_fixture(root)
            output = root / "dist"
            archive = build_wizard_release(root, output, VERSION)
            original = archive.read_bytes()
            for marker in (b"[Menu]", b";version='24.09.29.12'\n;version='24.09.29.12'"):
                with self.subTest(marker=marker):
                    start_ini.write_bytes(marker)
                    with self.assertRaisesRegex(ValueError, "expected one version marker"):
                        build_wizard_release(root, output, VERSION)
                    self.assertEqual(archive.read_bytes(), original)
            start_ini.unlink()
            with self.assertRaisesRegex(ValueError, "missing Wizard payload files"):
                build_wizard_release(root, output, VERSION)
            self.assertEqual(archive.read_bytes(), original)

    def test_invalid_versions_are_rejected_before_packaging(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            for version in ("26.11.24.7", "../archive", "26.02.30.07", "26.11.24.24"):
                with self.subTest(version=version):
                    with self.assertRaises(ValueError):
                        build_wizard_release(root, root / "dist", version)
            self.assertFalse((root / "dist").exists())


if __name__ == "__main__":
    unittest.main()

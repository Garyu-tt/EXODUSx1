"""Host-side regression tests for the pinned FreeBSD manager input port."""

from __future__ import annotations

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

import prepare_manager_user_data as prepare


HERE = Path(__file__).resolve().parent
TEMPLATE = HERE / "guest-manager.sh"
PATCH = HERE / "patches" / "steamos-manager-freebsd-input.patch"


class ManagerInputPatchTests(unittest.TestCase):
    def test_actual_patch_is_embedded_and_shell_remains_valid(self):
        rendered = prepare.render(TEMPLATE, PATCH)
        text = rendered.decode("utf-8")
        self.assertNotIn(prepare.MARKER, text)
        self.assertIn("input-linux-sys-0.9.0/src/freebsd.rs", text)
        self.assertIn("#[cfg(target_os = \"freebsd\")]", text)
        self.assertIn("#[cfg(not(target_os = \"freebsd\"))]", text)
        shell = shutil.which("sh")
        if shell is not None:
            with tempfile.TemporaryDirectory() as temporary:
                user_data = Path(temporary) / "user-data"
                user_data.write_bytes(rendered)
                subprocess.run([shell, "-n", str(user_data)], check=True)

    def test_patch_scope_is_exact_and_keeps_linux_bindings(self):
        text = PATCH.read_text(encoding="utf-8")
        paths = [
            line.split()[2][2:]
            for line in text.splitlines()
            if line.startswith("diff --git ")
        ]
        self.assertEqual(
            paths,
            [
                "input-linux-sys-0.9.0/src/input.rs",
                "input-linux-sys-0.9.0/src/freebsd.rs",
                "input-linux-0.7.1/src/evdev.rs",
                "input-linux-0.7.1/src/macros.rs",
            ],
        )
        self.assertIn(
            '+        #[cfg(not(target_os = "freebsd"))]\n'
            "         pub fn $name_mask(&self)",
            text,
        )
        self.assertIn('+#[cfg(not(target_os = "freebsd"))]\n pub use libc::{', text)
        self.assertIn("assert!(size_of::<input_event>() == 24);", text)
        self.assertIn("assert!(size_of::<ff_effect>() == 48);", text)
        self.assertNotIn("+pub struct input_mask", text)

    def test_template_and_host_gate_remain_strict(self):
        template = TEMPLATE.read_text(encoding="utf-8")
        runner = (HERE / "run-reference.sh").read_text(encoding="utf-8")
        self.assertIn("cargo check --locked -p steamos-manager", template)
        self.assertIn('EXODUS_MANAGER_DAEMON_CHECK_RESULT=$manager_rc', template)
        self.assertIn('count == 1 && !invalid', runner)
        self.assertIn("expected exactly one result=0", runner)

    def test_rejects_missing_duplicate_and_terminating_payloads(self):
        patch = PATCH.read_text(encoding="utf-8")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            patch_path = root / "patch"
            patch_path.write_text(patch, encoding="utf-8")
            for template in ("#!/bin/sh\n", f"{prepare.MARKER}\n{prepare.MARKER}\n"):
                template_path = root / "template"
                template_path.write_text(template, encoding="utf-8")
                with self.assertRaises(prepare.PreparationError):
                    prepare.render(template_path, patch_path)
            template_path.write_text(f"#!/bin/sh\n{prepare.MARKER}\n", encoding="utf-8")
            patch_path.write_text(patch + f"\n{prepare.HEREDOC_END}\n", encoding="utf-8")
            with self.assertRaises(prepare.PreparationError):
                prepare.render(template_path, patch_path)

    def test_atomic_writer_replaces_complete_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "user-data"
            output.write_bytes(b"old")
            payload = prepare.render(TEMPLATE, PATCH)
            prepare.write_atomic(output, payload)
            self.assertEqual(output.read_bytes(), payload)
            self.assertEqual(list(output.parent.glob(".user-data.*")), [])


if __name__ == "__main__":
    unittest.main()

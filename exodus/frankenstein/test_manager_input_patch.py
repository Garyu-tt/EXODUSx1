"""Host-side regression tests for the pinned FreeBSD manager input port."""

from __future__ import annotations

import hashlib
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

import prepare_manager_user_data as prepare


HERE = Path(__file__).resolve().parent
TEMPLATE = HERE / "guest-manager.sh"
PATCH = HERE / "patches" / "steamos-manager-freebsd-input.patch"
PLATFORM_PATCH = HERE / "patches" / "steamos-manager-freebsd-platform.patch"
WORKFLOW = HERE.parent.parent / ".github" / "workflows" / "frankenstein-bsd.yml"


class ManagerInputPatchTests(unittest.TestCase):
    def test_actual_patch_is_embedded_and_shell_remains_valid(self):
        rendered = prepare.render(TEMPLATE, PATCH, PLATFORM_PATCH)
        text = rendered.decode("utf-8")
        self.assertNotIn(prepare.MARKER, text)
        self.assertNotIn(prepare.PLATFORM_MARKER, text)
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
                "input-linux-0.7.1/src/evdev.rs",
                "input-linux-0.7.1/src/macros.rs",
                "input-linux-0.7.1/src/uinput.rs",
                "input-linux-sys-0.9.0/src/freebsd.rs",
                "input-linux-sys-0.9.0/src/input.rs",
                "input-linux-sys-0.9.0/src/uinput.rs",
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

    def test_freebsd_gates_incompatible_ioctl_surface(self):
        text = PATCH.read_text(encoding="utf-8")
        fragments = (
            '+#[cfg(not(target_os = "freebsd"))]\n ioctl_read! {\n'
            " \t/// get keycode\n \tev_get_keycode,",
            '+#[cfg(not(target_os = "freebsd"))]\n ioctl_read! {\n'
            " \t/// get keycode\n \tev_get_keycode_v2,",
            '/// `EVIOCGKEYCODE`\n+            #[cfg(not(target_os = "freebsd"))]\n'
            "             @get keycode_legacy",
            '/// `EVIOCGKEYCODE_V2`\n+            #[cfg(not(target_os = "freebsd"))]\n'
            "             @get keycode",
            "/// get MT slot values\n+#[cfg(not(target_os = \"freebsd\"))]\n"
            " pub unsafe fn ev_get_mtslots",
            "/// set abs value/limits\n+#[cfg(not(target_os = \"freebsd\"))]\n"
            " pub unsafe fn ev_set_abs",
            '/// `EVIOCGMTSLOTS`\n     ///\n'
            "     /// Warning: the current implementation can leak uninitialized heap memory into `values`\n"
            '+    #[cfg(not(target_os = "freebsd"))]\n'
            "     pub fn multi_touch_slots",
            '/// `EVIOCSABS`\n+    #[cfg(not(target_os = "freebsd"))]\n'
            "     pub fn set_absolute_info",
            '+#[cfg(not(target_os = "freebsd"))]\n'
            " ioctl_write_ptr!(ui_set_phys, b'U', 108, c_char);",
            '/// `UI_SET_PHYS`\n+            #[cfg(not(target_os = "freebsd"))]\n'
            "             @set_str set_phys",
            "Note that this path may not exist if sysfs is not mounted in the standard `/sys` location.\n"
            '+    #[cfg(not(target_os = "freebsd"))]\n'
            "     pub fn sys_path",
            "/// The device name of the input device.\n"
            '+    #[cfg(not(target_os = "freebsd"))]\n'
            "     pub fn evdev_name",
            "Note that this path may not exist if `/dev/input/*` isn't mounted properly.\n"
            '+    #[cfg(not(target_os = "freebsd"))]\n'
            "     pub fn evdev_path",
            '/// `UI_GET_SYSNAME`\n+            #[cfg(not(target_os = "freebsd"))]\n'
            "             @get_str sys_name, sys_name_buf",
            '+#[cfg(not(target_os = "freebsd"))]\n'
            " impl<'a> From<&'a ff_effect> for &'a ff_effect_union",
            '+#[cfg(not(target_os = "freebsd"))]\n'
            " impl<'a> From<&'a mut ff_effect> for &'a mut ff_effect_union",
        )
        for fragment in fragments:
            with self.subTest(fragment=fragment):
                self.assertIn(fragment, text)
        preserved = (
            '+#[cfg(not(target_os = "freebsd"))]\n pub unsafe fn ev_get_abs',
            '+#[cfg(not(target_os = "freebsd"))]\n ioctl_write_ptr! {\n \t/// set keycode',
            '+#[cfg(not(target_os = "freebsd"))]\n ioctl_write_int!(ui_set_evbit',
            '+#[cfg(not(target_os = "freebsd"))]\n ioctl_write_int!(ui_set_keybit',
            '+#[cfg(not(target_os = "freebsd"))]\n ioctl_write_ptr! {\n \t/// Set device parameters',
        )
        for fragment in preserved:
            with self.subTest(preserved=fragment):
                self.assertNotIn(fragment, text)

    def test_platform_patch_scope_and_dependency_contract_are_exact(self):
        text = PLATFORM_PATCH.read_text(encoding="utf-8")
        paths = [
            line.split()[2][2:]
            for line in text.splitlines()
            if line.startswith("diff --git ")
        ]
        self.assertEqual(
            paths,
            [
                "Cargo.lock",
                "steamos-manager/Cargo.toml",
                "steamos-manager/examples/freebsd_inotify_probe.rs",
                "steamos-manager/src/bin/steamosctl.rs",
                "steamos-manager/src/cec.rs",
                "steamos-manager/src/cec_types.rs",
                "steamos-manager/src/hardware.rs",
                "steamos-manager/src/lib.rs",
                "steamos-manager/src/manager/user.rs",
                "steamos-manager/src/power.rs",
            ],
        )
        for fragment in (
            'version = "0.11.4"',
            'checksum = "153be1941a183ec9ccd095ddbe17a8b8d435ef6c76e9e02451b933c3999af2c8"',
            'version = "0.1.8"',
            'checksum = "c033f80b2c113cdf91ab7a33faa9cbc014726dcad99880c8609af2a370edf37d"',
            "[target.'cfg(not(target_os = \"freebsd\"))'.dependencies]",
            "pub(crate) mod cec_types;",
            "no CEC device or ioctl",
            "libc::dup(original.as_raw_fd())",
            "original_watch, duplicate_watch",
            "EXODUS_MANAGER_INOTIFY_RUST_PASS",
            "Errno::ENODATA",
            "CLOCK_MONOTONIC_RAW",
            "screen-reader action timestamps are not supported on FreeBSD",
        ):
            with self.subTest(fragment=fragment):
                self.assertIn(fragment, text)
        self.assertNotIn("Errno::ENOENT", text)
        self.assertNotIn("Errno::ENOATTR", text)
        self.assertNotIn("CLOCK_MONOTONIC_PRECISE", text)
        self.assertNotIn("tokio::time", text)

    def test_official_crate_hash_and_embedded_patch_hash_are_pinned(self):
        template = TEMPLATE.read_text(encoding="utf-8")
        patch_hash = hashlib.sha256(PATCH.read_bytes()).hexdigest()
        platform_hash = hashlib.sha256(PLATFORM_PATCH.read_bytes()).hexdigest()
        self.assertIn(f'test "$(sha256 -q "$input_patch")" = {patch_hash}', template)
        self.assertIn(f'test "$(sha256 -q "$platform_patch")" = {platform_hash}', template)
        self.assertIn(
            'test "$(sha256 -q Cargo.lock)" = '
            "001bf56bb9f357710986bea869c67473e2afd358bf2288a00b661ba8ea0027c1",
            template,
        )
        self.assertIn(
            'test "$(sha256 -q Cargo.lock)" = '
            "6035e3ec787fbad93d3dfb45f2ebc93d9b53139484cd572603ca8b1e1220da2e",
            template,
        )
        self.assertIn(
            "e1a14a5fb13c2900df9065126b21b2aa0b803c1e7d95349b34fd2df37de944c4",
            template,
        )
        self.assertNotIn(
            "7b64aa98481fa7340ace5d81abfe4d3e3c453ee8096b0b0d75d868f00b3e84a2",
            template,
        )

    def test_template_and_host_gate_remain_strict(self):
        template = TEMPLATE.read_text(encoding="utf-8")
        runner = (HERE / "run-reference.sh").read_text(encoding="utf-8")
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("cargo check --locked -p steamos-manager", template)
        self.assertIn("cargo build --locked -p steamos-manager", template)
        self.assertIn("timeout 15s target/debug/examples/freebsd_inotify_probe", template)
        self.assertIn("EXODUS_MANAGER_FREEBSD_DEPENDENCY_TREE_PASS", runner)
        self.assertIn("EXODUS_MANAGER_DAEMON_BUILD_PASS", runner)
        self.assertIn("EXODUS_MANAGER_INOTIFY_RUST_PASS", runner)
        self.assertIn('EXODUS_MANAGER_DAEMON_CHECK_RESULT=$manager_rc', template)
        self.assertIn('count == 1 && !invalid', runner)
        self.assertIn("expected exactly one result=0", runner)
        self.assertIn("supports the FreeBSD uinput operations used by", template)
        self.assertIn("Other input-linux helpers are outside that runtime claim", template)
        self.assertIn("set -euxo pipefail", workflow)
        self.assertIn(
            "timeout 15s target/debug/examples/freebsd_inotify_probe | tee", workflow
        )

    def test_rejects_missing_duplicate_and_terminating_payloads(self):
        patch = PATCH.read_text(encoding="utf-8")
        platform_patch = PLATFORM_PATCH.read_text(encoding="utf-8")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            patch_path = root / "patch"
            platform_patch_path = root / "platform-patch"
            patch_path.write_text(patch, encoding="utf-8")
            platform_patch_path.write_text(platform_patch, encoding="utf-8")
            templates = (
                "#!/bin/sh\n",
                f"{prepare.MARKER}\n{prepare.MARKER}\n{prepare.PLATFORM_MARKER}\n",
                f"{prepare.MARKER}\n{prepare.PLATFORM_MARKER}\n{prepare.PLATFORM_MARKER}\n",
            )
            for template in templates:
                template_path = root / "template"
                template_path.write_text(template, encoding="utf-8")
                with self.assertRaises(prepare.PreparationError):
                    prepare.render(template_path, patch_path, platform_patch_path)
            template_path.write_text(
                f"#!/bin/sh\n{prepare.MARKER}\n{prepare.PLATFORM_MARKER}\n",
                encoding="utf-8",
            )
            patch_path.write_text(patch + f"\n{prepare.HEREDOC_END}\n", encoding="utf-8")
            with self.assertRaises(prepare.PreparationError):
                prepare.render(template_path, patch_path, platform_patch_path)
            patch_path.write_text(patch, encoding="utf-8")
            platform_patch_path.write_text(
                platform_patch + f"\n{prepare.PLATFORM_HEREDOC_END}\n", encoding="utf-8"
            )
            with self.assertRaises(prepare.PreparationError):
                prepare.render(template_path, patch_path, platform_patch_path)

            for marker in (prepare.INPUT_MARKER, prepare.PLATFORM_MARKER):
                with self.subTest(payload_marker=marker):
                    patch_path.write_text(patch + f"\n{marker}\n", encoding="utf-8")
                    platform_patch_path.write_text(platform_patch, encoding="utf-8")
                    with self.assertRaises(prepare.PreparationError):
                        prepare.render(template_path, patch_path, platform_patch_path)

                    patch_path.write_text(patch, encoding="utf-8")
                    platform_patch_path.write_text(
                        platform_patch + f"\n{marker}\n", encoding="utf-8"
                    )
                    with self.assertRaises(prepare.PreparationError):
                        prepare.render(template_path, patch_path, platform_patch_path)

    def test_rejects_extra_or_mismatched_platform_patch_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            platform_patch = root / "platform-patch"
            original = PLATFORM_PATCH.read_text(encoding="utf-8")
            invalid_patches = (
                original + "diff --git a/unexpected b/unexpected\n",
                original.replace(
                    "diff --git a/Cargo.lock b/Cargo.lock",
                    "diff --git a/unexpected b/Cargo.lock",
                    1,
                ),
            )
            for invalid in invalid_patches:
                with self.subTest(header=invalid.splitlines()[0]):
                    platform_patch.write_text(invalid, encoding="utf-8")
                    with self.assertRaises(prepare.PreparationError):
                        prepare.render(TEMPLATE, PATCH, platform_patch)

    def test_atomic_writer_replaces_complete_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "user-data"
            output.write_bytes(b"old")
            payload = prepare.render(TEMPLATE, PATCH, PLATFORM_PATCH)
            prepare.write_atomic(output, payload)
            self.assertEqual(output.read_bytes(), payload)
            self.assertEqual(list(output.parent.glob(".user-data.*")), [])


if __name__ == "__main__":
    unittest.main()

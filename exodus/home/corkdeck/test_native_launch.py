"""Host regressions for actual Windows/Steam handoff admission, not device execution."""

from pathlib import Path
import os
import shlex
import struct
import subprocess
import tempfile
import unittest

SOURCE_DIR = Path(__file__).resolve().parent
RUNTIME = "sdmc:/switch/wine/wine-nx-runtime.nro"
TARGET = "sdmc:/Games/My Game.EXE"
QUEUE = "sdmc:/switch/wine/run-next.txt"

# Only Horizon entrypoints are substituted. The production native adapter,
# Autorun NRO preflight and PE validator are compiled and exercised unchanged.
SWITCH_STUB = r"""
#ifndef EXODUS_QA_SWITCH_STUB_H
#define EXODUS_QA_SWITCH_STUB_H
#include <stdbool.h>
#include <stdint.h>
typedef uint32_t Result;
typedef int32_t s32;
typedef enum {
    AppletType_None = 0,
    AppletType_Application = 1,
    AppletType_SystemApplication = 2,
    AppletType_LibraryApplet = 3
} AppletType;
typedef struct { uint64_t application_id; } NsApplicationRecord;
#define R_FAILED(value) ((value) != 0)
Result nsInitialize(void);
Result nsListApplicationRecord(NsApplicationRecord *, s32, s32, s32 *);
void nsExit(void);
AppletType appletGetAppletType(void);
bool envHasNextLoad(void);
bool hosversionAtLeast(int, int, int);
Result appletRequestLaunchApplication(uint64_t, const void *);
Result envSetNextLoad(const char *, const char *);
#endif
"""

NATIVE_HARNESS = r"""
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <switch.h>
#include "native_launch.h"

static AppletType mode;
static int loader_available;
static Result next_result;
static unsigned next_calls;
static char recorded_path[1024];
static char recorded_args[1024];

Result nsInitialize(void) { return 1; }
Result nsListApplicationRecord(NsApplicationRecord *records, s32 count, s32 offset, s32 *actual)
{
    (void)records; (void)count; (void)offset; *actual = 0; return 1;
}
void nsExit(void) {}
AppletType appletGetAppletType(void) { return mode; }
bool envHasNextLoad(void) { return loader_available != 0; }
bool hosversionAtLeast(int major, int minor, int patch)
{
    (void)major; (void)minor; (void)patch; return false;
}
Result appletRequestLaunchApplication(uint64_t id, const void *argument)
{
    (void)id; (void)argument; return 1;
}
Result envSetNextLoad(const char *path, const char *args)
{
    ++next_calls;
    snprintf(recorded_path, sizeof(recorded_path), "%s", path);
    snprintf(recorded_args, sizeof(recorded_args), "%s", args);
    return next_result;
}

int main(int argc, char **argv)
{
    if (argc != 6) return 2;
    const enum exodus_route route = !strcmp(argv[1], "windows") ? EXODUS_ROUTE_WINDOWS : EXODUS_ROUTE_STEAM;
    mode = (AppletType)strtol(argv[3], NULL, 10);
    loader_available = (int)strtol(argv[4], NULL, 10);
    next_result = (Result)strtoul(argv[5], NULL, 10);
    char message[256] = {0};
    const int accepted = exodus_launch(route, argv[2], message);
    printf("%d\n%u\n%s\n%s\n%s\n", accepted, next_calls, recorded_path, recorded_args, message);
    return 0;
}
"""

def nro_image():
    image = bytearray(0x3000)
    image[0x10:0x14] = b"NRO0"
    struct.pack_into("<I", image, 0x18, len(image))
    struct.pack_into("<6I", image, 0x20, 0, 0x1000, 0x1000, 0x1000, 0x2000, 0x1000)
    return image


def pe_image(pe32_plus=False):
    """One bounded section in a structurally valid PE32 or PE32+ application."""
    image = bytearray(1024)
    image[:2] = b"MZ"
    struct.pack_into("<I", image, 0x3c, 0x80)
    image[0x80:0x84] = b"PE\0\0"
    optional_size = 240 if pe32_plus else 224
    struct.pack_into("<HH", image, 0x84, 0x8664 if pe32_plus else 0x14c, 1)
    struct.pack_into("<HH", image, 0x94, optional_size, 0x0002)
    optional = 0x98
    struct.pack_into("<H", image, optional, 0x20b if pe32_plus else 0x10b)
    struct.pack_into("<I", image, optional + 60, 512)
    struct.pack_into("<I", image, optional + (108 if pe32_plus else 92), 16)
    section = optional + optional_size
    image[section:section + 5] = b".text"
    struct.pack_into("<II", image, section + 16, 512, 512)
    return image

@unittest.skipUnless(os.name == "posix", "Requires POSIX paths for the fake SD mount")
class NativeLaunchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory(prefix="exodus-native-launch-")
        cls.addClassCleanup(build.cleanup)
        build_dir = Path(build.name)
        (build_dir / "switch.h").write_text(SWITCH_STUB)
        harness = build_dir / "native_route_harness.c"
        harness.write_text(NATIVE_HARNESS)
        cls.binary = build_dir / "native_route"
        command = shlex.split(os.environ.get("CC", "cc"))
        command += ["-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Werror"]
        command += shlex.split(os.environ.get("CFLAGS", ""))
        command += ["-D__SWITCH__", "-I" + str(build_dir), "-I" + str(SOURCE_DIR),
                    str(harness), str(SOURCE_DIR.parent.parent / "runtime" / "autorun_probe.c"),
                    str(SOURCE_DIR / "windows_pe.c"), "-o", str(cls.binary)]
        subprocess.run(command, check=True)

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for relative, data in [(RUNTIME, nro_image()), (TARGET, pe_image())]:
            path = self.root / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)

    def launch(self, target=TARGET, route="windows", mode=1, loader=1, result=0):
        before = {str(p.relative_to(self.root)): p.read_bytes()
                  for p in self.root.rglob("*") if p.is_file()}
        completed = subprocess.run(
            [self.binary, route, target, str(mode), str(loader), str(result)],
            cwd=self.root, capture_output=True, encoding="utf-8", check=True)
        self.assertEqual(completed.stderr, "")
        self.assertEqual(before, {str(p.relative_to(self.root)): p.read_bytes()
                                  for p in self.root.rglob("*") if p.is_file()})
        accepted, calls, path, args, message = completed.stdout.splitlines()
        return int(accepted), int(calls), path, args, message

    def test_pe32_and_pe32_plus_targets_are_quoted_exactly_once(self):
        for pe32_plus in (False, True):
            with self.subTest(pe32_plus=pe32_plus):
                (self.root / TARGET).write_bytes(pe_image(pe32_plus))
                accepted, calls, path, args, message = self.launch()
                self.assertEqual((accepted, calls, path, args, message),
                                 (1, 1, RUNTIME, RUNTIME + ' "' + TARGET + '"', ""))

    def test_malformed_pe_is_refused_before_handoff(self):
        for content in (b"MZ", b"MZ" + bytes(126), pe_image()[:151], pe_image()[:-1]):
            with self.subTest(size=len(content)):
                (self.root / TARGET).write_bytes(content)
                accepted, calls, _, _, message = self.launch()
                self.assertEqual((accepted, calls), (0, 0))
                self.assertIn("PE", message)

    def test_unicode_and_space_target_bytes_are_preserved(self):
        target = "sdmc:/Games/Русский 猫.exe"
        (self.root / target).write_bytes(pe_image(True))
        accepted, calls, path, args, _ = self.launch(target=target)
        self.assertEqual((accepted, calls, path, args),
                         (1, 1, RUNTIME, RUNTIME + ' "' + target + '"'))

    def test_application_mode_and_loader_are_required(self):
        for mode, loader in [(0, 1), (2, 1), (3, 1), (1, 0)]:
            with self.subTest(mode=mode, loader=loader):
                accepted, calls, _, _, message = self.launch(mode=mode, loader=loader)
                self.assertEqual((accepted, calls), (0, 0))
                self.assertIn("application mode", message)

    def test_bad_paths_never_reach_loader(self):
        for target in ["ums0:/Games/a.exe", "sdmc:/Games/../a.exe", "sdmc:/Games/a\"b.exe",
                       "sdmc:/Games/a\\b.exe", "sdmc:/Games//a.exe", "sdmc:/Games/a\n.exe"]:
            with self.subTest(target=target):
                accepted, calls, _, _, message = self.launch(target=target)
                self.assertEqual((accepted, calls), (0, 0))
                self.assertIn("Invalid target", message)

    def test_missing_or_invalid_runtime_never_reaches_loader(self):
        runtime = self.root / RUNTIME
        runtime.unlink()
        self.assertEqual(self.launch()[:2], (0, 0))
        runtime.write_bytes(b"NRO0")
        self.assertEqual(self.launch()[:2], (0, 0))

    def test_existing_queue_blocks_windows_and_steam(self):
        queue = self.root / QUEUE
        for directory in (False, True):
            if directory:
                queue.mkdir()
            else:
                queue.write_bytes(b"existing job\n")
            for route, target in [("windows", TARGET), ("steam", "42")]:
                with self.subTest(directory=directory, route=route):
                    accepted, calls, _, _, message = self.launch(target=target, route=route)
                    self.assertEqual((accepted, calls), (0, 0))
                    self.assertIn("Pending runtime job", message)
            queue.rmdir() if directory else queue.unlink()

    def test_unavailable_or_unrecognized_executable_never_reaches_loader(self):
        target = self.root / TARGET
        target.unlink()
        self.assertEqual(self.launch()[:2], (0, 0))
        target.mkdir()
        self.assertEqual(self.launch()[:2], (0, 0))
        target.rmdir()
        for content in (b"", b"M", b"NO"):
            target.write_bytes(content)
            self.assertEqual(self.launch()[:2], (0, 0))

    def test_loader_rejection_keeps_route_failed(self):
        accepted, calls, _, _, message = self.launch(result=0x123)
        self.assertEqual((accepted, calls), (0, 1))
        self.assertIn("0x00000123", message)

    def test_steam_opens_runtime_library_without_appid_argument(self):
        (self.root / TARGET).unlink()
        accepted, calls, path, args, _ = self.launch(route="steam", target="4294967295")
        self.assertEqual((accepted, calls, path, args), (1, 1, RUNTIME, RUNTIME))
        for target in ["0", "042", "4294967296", "42;other", ""]:
            with self.subTest(target=target):
                self.assertEqual(self.launch(route="steam", target=target)[:2], (0, 0))

if __name__ == "__main__":
    unittest.main()

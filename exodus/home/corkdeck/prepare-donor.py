#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Stage the exact CorkDeck source subset used by the EXODUS Shell."""

import argparse
import hashlib
from pathlib import Path
import subprocess


PIN = "adc0b52284e9e57d18808427a770a2a162c76bf0"
SOURCE_DIR = Path("horizon-wine/source")
MARKER = Path("EXODUS-DONOR-PIN")
HERE = Path(__file__).resolve().parent
FILES = (
    "launcher_audio.c", "launcher_audio.h", "launcher_image.c", "launcher_image.h",
    "launcher_pe.h", "launcher_svg.c", "launcher_svg.h", "launcher_ui.c", "launcher_ui.h",
)
MODIFICATIONS = b"""The upstream source is CorkDeck commit adc0b52284e9e57d18808427a770a2a162c76bf0:
https://github.com/plv890/CorkDeck/tree/adc0b52284e9e57d18808427a770a2a162c76bf0

The terms for this prepared copy are the GNU General Public License, version 2.
The EXODUSx1 contributors modified launcher_ui.c and launcher_ui.h on
2026-11-25 to remove the prior focus implementation and connect a separately
written stationary SDL frame. The upstream authorship notices are retained.
"""
FOCUS_NOTICE = (
    b"/* Modified by EXODUSx1 contributors on 2026-11-25 to use a separate focus frame. */\n"
)


def git(source, *args):
    return subprocess.check_output(["git", "-C", str(source), *args])


def replace_region(data, first, following, replacement):
    if data.count(first) != 1 or data.count(following) != 1:
        raise ValueError("Unexpected CorkDeck focus region in pinned source")
    start = data.index(first)
    stop = data.index(following, start + len(first))
    return data[:start] + replacement + data[stop:]


def elect_gpl2(license_notice):
    # LGPL 2.1 section 3 changes the license references in this copy.
    return (license_notice.replace(b"GNU Lesser General Public License", b"GNU General Public License")
            .replace(b"GNU\nLesser General Public License", b"GNU\nGeneral Public License")
            .replace(b"either version 2.1 of the License, or (at\nyour option) any later version. ",
                     b"version 2 of the License.")
            .replace(b"COPYING.LIB", b"COPYING"))


def checkout_snapshot(source):
    if git(source, "rev-parse", "HEAD").strip().decode("ascii") != PIN:
        raise ValueError(f"CorkDeck checkout must be pinned to {PIN}")
    git(source, "diff", "--quiet", "HEAD", "--", SOURCE_DIR.as_posix(), "AUTHORS", "LICENSE")
    result = {}
    for name in FILES:
        path = SOURCE_DIR / name
        data = git(source, "show", f"{PIN}:{path.as_posix()}")
        if name == "launcher_ui.c":
            data = replace_region(data, b"/* The border tico-nx draws around ",
                                  b"void ui_fill_circle(", b"")
        elif name == "launcher_ui.h":
            data = replace_region(data, b"/* What has the focus:", b"void ui_animated_border(",
                                  b"/* The active control's stationary focus frame. */\n")
            data = data.replace(b"/* The same outline with nothing travelling round it. */",
                                b"/* A stationary outline in one color. */")
        if name in ("launcher_ui.c", "launcher_ui.h"):
            data = FOCUS_NOTICE + data.replace(b"ui_animated_border(", b"ui_focus_border(")
        result[path] = b"// SPDX-License-Identifier: GPL-2.0-only\n" + data
    result[Path("AUTHORS")] = git(source, "show", f"{PIN}:AUTHORS")
    result[Path("COPYING")] = (HERE.parent / "ulaunch" / "COPYING").read_bytes().replace(b"\r\n", b"\n")
    result[Path("LICENSE")] = elect_gpl2(git(source, "show", f"{PIN}:LICENSE"))
    result[Path("EXODUS-MODIFICATIONS")] = MODIFICATIONS
    result[MARKER] = (PIN + "\n").encode("ascii")
    return result


def snapshot_manifest():
    manifest = {}
    for line in (HERE / "donor-snapshot.sha256").read_text(encoding="ascii").splitlines():
        digest, name = line.split("  ", 1)
        manifest[Path(name)] = digest
    return manifest


def prepare(source, output):
    expected = snapshot_manifest()
    if (source / MARKER).is_file():
        snapshot = {path: (source / path).read_bytes() for path in expected}
    else:
        snapshot = checkout_snapshot(source)
    actual = {path: hashlib.sha256(data).hexdigest() for path, data in snapshot.items()}
    if set(actual) != set(expected):
        raise ValueError("Incomplete pinned CorkDeck source subset")
    for path, digest in actual.items():
        if digest != expected[path]:
            raise ValueError(f"CorkDeck source digest mismatch for {path.as_posix()}")
    output.mkdir(parents=True)
    for path, data in snapshot.items():
        target = output / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="pinned Git checkout or prepared donor snapshot")
    parser.add_argument("output", type=Path, help="new staging directory")
    args = parser.parse_args()
    try:
        prepare(args.source, args.output)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"CorkDeck source preparation failed: {error}\n")


if __name__ == "__main__":
    main()

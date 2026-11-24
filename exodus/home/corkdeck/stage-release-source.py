#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Export the exact shell build inputs from a Git commit."""

import argparse
import io
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile


REPO = Path(__file__).resolve().parents[3]
SHELL = Path("exodus/home/corkdeck")
INPUTS = (
    SHELL.as_posix(),
    "exodus/home/ulaunch/COPYING", "exodus/home/ulaunch/catalog.cpp",
    "exodus/home/ulaunch/catalog.hpp", "exodus/runtime/autorun_probe.c",
    "exodus/runtime/autorun_probe.h", ".github/workflows/exodus-corkdeck.yml",
)


def extract_inputs(data, output):
    output.mkdir(parents=True)
    output = output.resolve()
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:") as archive:
        for item in archive:
            target = (output / item.name).resolve()
            if not target.is_relative_to(output) or not (item.isdir() or item.isfile()):
                raise ValueError(f"Unexpected path in shell Git archive: {item.name}")
            if item.isdir():
                target.mkdir(parents=True, exist_ok=True)
            else:
                target.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(item) as source, target.open("wb") as destination:
                    shutil.copyfileobj(source, destination)
                target.chmod(item.mode & 0o777)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("revision", help="Git commit to export")
    parser.add_argument("donor", type=Path, help="pinned CorkDeck source")
    parser.add_argument("output", type=Path, help="new source distribution tree")
    args = parser.parse_args()

    git = ["git", "-C", str(REPO)]
    revision = subprocess.check_output(
        [*git, "rev-parse", "--verify", "--end-of-options", args.revision], text=True
    ).strip()
    if subprocess.check_output([*git, "cat-file", "-t", revision]).strip() != b"commit":
        parser.error("revision must identify a Git commit")
    data = subprocess.check_output([*git, "archive", revision, "--", *INPUTS])
    extract_inputs(data, args.output)
    subprocess.run(
        [sys.executable, str(args.output / SHELL / "prepare-donor.py"),
         str(args.donor.resolve()), str(args.output / "corkdeck-donor")],
        check=True,
    )
    shutil.copyfile(args.output / SHELL / "release-source.md", args.output / "BUILD_SOURCE.md")
    (args.output / "EXODUS-SHELL-COMMIT").write_text(revision + "\n", encoding="ascii")


if __name__ == "__main__":
    main()

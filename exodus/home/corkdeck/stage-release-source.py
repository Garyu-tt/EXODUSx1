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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("revision", help="Git commit to export")
    parser.add_argument("donor", type=Path, help="pinned CorkDeck source")
    parser.add_argument("output", type=Path, help="new source distribution tree")
    args = parser.parse_args()

    git = ["git", "-C", str(REPO)]
    revision = subprocess.check_output(
        [*git, "rev-parse", "--verify", f"{args.revision}^{{commit}}"], text=True
    ).strip()
    data = subprocess.check_output([*git, "archive", revision, "--", *INPUTS])
    args.output.mkdir(parents=True)
    with tarfile.open(fileobj=io.BytesIO(data), mode="r:") as archive:
        archive.extractall(args.output, filter="data")
    subprocess.run(
        [sys.executable, str(args.output / SHELL / "prepare-donor.py"),
         str(args.donor.resolve()), str(args.output / "corkdeck-donor")],
        check=True,
    )
    shutil.copyfile(args.output / SHELL / "release-source.md", args.output / "BUILD_SOURCE.md")
    (args.output / "EXODUS-SHELL-COMMIT").write_text(revision + "\n", encoding="ascii")


if __name__ == "__main__":
    main()

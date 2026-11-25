#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Fetch the pinned sources and stage the shell's dependency materials."""

import argparse
from pathlib import Path
import tarfile

from dependency_sources import fetch_dependencies


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--offline", action="store_true", help="use only verified cached inputs")
    args = parser.parse_args()
    try:
        fetch_dependencies(args.cache_dir, args.output_dir, args.offline)
    except (OSError, ValueError, KeyError, tarfile.TarError) as error:
        parser.exit(1, f"{error}\n")
    print(f"Verified dependency sources: {args.output_dir}")


if __name__ == "__main__":
    main()

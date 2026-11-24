#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Build and verify the paired CorkDeck shell binary and source archives."""

import argparse
import gzip
import hashlib
import io
import json
import re
import shutil
import subprocess
import sys
import tarfile
import zipfile
from pathlib import Path

from dependency_sources import (
    SHA256,
    checksum_text,
    list_files,
    load_manifest,
    relative_path,
    sha256,
    staged_output,
    verify_dependencies,
)


SOURCE_NAME = "EXODUS-Shell-Source.tar.gz"
BINARY_NAME = "EXODUS-Shell-Switch-arm64.zip"
SOURCE_ROOT = "EXODUS-Shell-Source"
NRO_INSTALL_PATH = "switch/exodus-shell/exodus-shell.nro"
CATALOG = "exodus/home/ulaunch"
SHELL = "exodus/home/corkdeck"
DONOR = "corkdeck-donor"
DONOR_DOCS = ("AUTHORS", "COPYING", "LICENSE", "EXODUS-DONOR-PIN", "EXODUS-MODIFICATIONS")


def verify_nro(path, checker):
    if path.is_symlink() or not path.is_file():
        raise ValueError("Expected a regular prebuilt NRO")
    subprocess.run([sys.executable, "-B", str(checker), str(path)], check=True)


def verify_donor(files):
    expected = {}
    for line in files[f"{SHELL}/donor-snapshot.sha256"].read_text(encoding="utf-8").splitlines():
        digest, separator, name = line.partition("  ")
        if not separator or not SHA256.fullmatch(digest):
            raise ValueError("Invalid checksum in staged donor manifest")
        name = relative_path(name).as_posix()
        if name in expected:
            raise ValueError(f"Duplicate donor manifest entry: {name}")
        expected[name] = digest

    prefix = f"{DONOR}/"
    donor_files = {name.removeprefix(prefix): value
                   for name, value in files.items() if name.startswith(prefix)}
    if donor_files.keys() != expected.keys():
        difference = sorted(donor_files.keys() ^ expected.keys())
        raise ValueError(f"Staged donor file set differs from the checksum manifest: {difference}")
    for name, path in donor_files.items():
        if sha256(path) != expected[name]:
            raise ValueError(f"Staged donor checksum mismatch: {name}")


def reviewed_source(root, donor_pin, main_sha):
    files = list_files(root)
    expected = {
        "BUILD_SOURCE.md",
        "EXODUS-SHELL-COMMIT",
        f"{CATALOG}/catalog.cpp",
        f"{CATALOG}/catalog.hpp",
        f"{CATALOG}/COPYING",
        f"{SHELL}/build-host.sh",
        f"{SHELL}/build-switch.sh",
        f"{SHELL}/check-release-nro.py",
        f"{SHELL}/check_shell_nro.c",
        f"{SHELL}/control_metrics.h",
        f"{SHELL}/donor-snapshot.sha256",
        f"{SHELL}/focus_adapter.c",
        f"{SHELL}/focus_indicator.c",
        f"{SHELL}/focus_indicator.h",
        f"{SHELL}/horizon_settings.h",
        f"{SHELL}/launch_route.h",
        f"{SHELL}/library_catalog.h",
        f"{SHELL}/library_filter.h",
        f"{SHELL}/live_inventory.hpp",
        f"{SHELL}/native_launch.h",
        f"{SHELL}/prepare-donor.py",
        f"{SHELL}/runtime_handoff.h",
        f"{SHELL}/shell_menu.h",
        f"{SHELL}/stage-release-source.py",
        f"{SHELL}/verify-repro.sh",
        f"{SHELL}/settings_preview.c",
        f"{SHELL}/library_catalog.cpp",
        f"{SHELL}/windows_pe.c",
        f"{SHELL}/windows_pe.h",
        "exodus/runtime/autorun_probe.c",
        "exodus/runtime/autorun_probe.h",
    }
    expected.update(f"{DONOR}/{name}" for name in DONOR_DOCS)
    donor_source = f"{DONOR}/horizon-wine/source/"
    expected.update(donor_source + f"launcher_{name}.c" for name in ("ui", "svg", "audio", "image"))
    missing = sorted(expected - files.keys())
    if missing:
        raise ValueError(f"Missing reviewed source files: {missing}")
    for name in expected:
        if files[name].stat().st_size == 0:
            raise ValueError(f"Empty reviewed source file: {name}")
    if files["EXODUS-SHELL-COMMIT"].read_text(encoding="ascii").strip() != main_sha:
        raise ValueError("Staged EXODUS commit does not match the requested main commit")
    pin = files[f"{DONOR}/EXODUS-DONOR-PIN"].read_text(encoding="utf-8").lower()
    if donor_pin not in pin.split():
        raise ValueError("Staged CorkDeck revision does not match the pinned donor")
    verify_donor(files)
    for name in files:
        path = relative_path(name)
        if name in {"RELEASE-IDENTITY.json", "SHA256SUMS"} or path.parts[0] == "dependencies":
            raise ValueError(f"Release material conflicts with staged source: {name}")
        if {".git", "__pycache__"} & set(path.parts) or path.suffix.lower() in {
            ".nro", ".nsp", ".elf", ".o", ".exe", ".dll", ".pyc", ".a"
        }:
            raise ValueError(f"Unexpected binary or checkout material in staged source: {name}")
        if name.startswith(donor_source) and path.suffix not in {".c", ".h"}:
            raise ValueError(f"Unexpected asset in staged donor source: {name}")
    return files


def byte_digest(value):
    return hashlib.sha256(value).hexdigest()


def entries_checksums(entries):
    return {name: sha256(value) if isinstance(value, Path) else byte_digest(value)
            for name, value in entries.items()}


def write_tar(path, entries):
    with path.open("wb") as file:
        with gzip.GzipFile(filename="", mode="wb", fileobj=file, mtime=0) as compressed:
            with tarfile.open(fileobj=compressed, mode="w|", format=tarfile.PAX_FORMAT) as archive:
                for name, value in sorted(entries.items()):
                    part = tarfile.TarInfo(f"{SOURCE_ROOT}/{name}")
                    part.mode = 0o755 if Path(name).suffix == ".sh" else 0o644
                    if isinstance(value, Path):
                        part.size = value.stat().st_size
                        with value.open("rb") as stream:
                            archive.addfile(part, stream)
                    else:
                        part.size = len(value)
                        archive.addfile(part, io.BytesIO(value))


def verify_tar(path, checksums):
    found = {}
    with tarfile.open(path, "r|gz") as archive:
        for member in archive:
            prefix = f"{SOURCE_ROOT}/"
            name = member.name.removeprefix(prefix)
            if not member.isfile() or not member.name.startswith(prefix) or name in found:
                raise ValueError(f"Unexpected source archive member: {member.name}")
            with archive.extractfile(member) as stream:
                found[name] = hashlib.file_digest(stream, "sha256").hexdigest()
    if found != checksums:
        raise ValueError("Completed source archive does not match its input files")


def write_zip(path, entries):
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_STORED) as archive:
        for name, value in sorted(entries.items()):
            part = zipfile.ZipInfo(name)
            part.create_system = 3
            part.external_attr = 0o100644 << 16
            with archive.open(part, "w") as target:
                if isinstance(value, Path):
                    with value.open("rb") as stream:
                        shutil.copyfileobj(stream, target)
                else:
                    target.write(value)


def verify_zip(path, checksums):
    found = {}
    with zipfile.ZipFile(path) as archive:
        for part in archive.infolist():
            if part.is_dir() or part.filename in found:
                raise ValueError(f"Unexpected binary archive member: {part.filename}")
            with archive.open(part) as stream:
                found[part.filename] = hashlib.file_digest(stream, "sha256").hexdigest()
    if found != checksums:
        raise ValueError("Completed binary archive does not match its input files")


def bundle(app_source, dependencies, main_sha, nro, output):
    if not re.fullmatch(r"[0-9a-f]{40}", main_sha):
        raise ValueError("Expected the full lowercase EXODUS main commit SHA")
    manifest = load_manifest()
    donor_pin = manifest["corkdeck_donor"]
    app = reviewed_source(app_source, donor_pin, main_sha)
    verify_nro(nro, app[f"{SHELL}/check-release-nro.py"])
    deps = verify_dependencies(dependencies, manifest)
    identity = {
        "format": 1,
        "exodus_main": main_sha,
        "corkdeck_donor": donor_pin,
        "native_image": manifest["native_image"],
        "nro_sha256": sha256(nro),
        "source_archive": SOURCE_NAME,
    }
    identity_bytes = (json.dumps(identity, indent=2, sort_keys=True) + "\n").encode("utf-8")
    source = {
        **app,
        **{f"dependencies/{name}": value for name, value in deps.items()},
        "RELEASE-IDENTITY.json": identity_bytes,
    }
    source_sums = entries_checksums(source)
    source["SHA256SUMS"] = checksum_text(source_sums).encode("utf-8")
    source_sums["SHA256SUMS"] = byte_digest(source["SHA256SUMS"])

    with staged_output(output) as stage:
        source_archive = stage / SOURCE_NAME
        write_tar(source_archive, source)
        verify_tar(source_archive, source_sums)
        source_sha = sha256(source_archive)
        donor_docs = {
            f"LICENSES/{name}": value for name, value in app.items()
            if name.startswith(f"{DONOR}/") and "/horizon-wine/" not in name
        }
        binary = {
            NRO_INSTALL_PATH: nro,
            "LICENSES/GPL-2.0-only.txt": app[f"{CATALOG}/COPYING"],
            **donor_docs,
            **{f"LICENSES/dependencies/{name.removeprefix('licenses/')}": value
               for name, value in deps.items() if name.startswith("licenses/")},
            "RELEASE-IDENTITY.json": identity_bytes,
            "SOURCE-SHA256": f"{source_sha}  {SOURCE_NAME}\n".encode("utf-8"),
            "README.txt": (
                "EXODUS Shell for Nintendo Switch\n"
                "Copy switch/exodus-shell to the SD card and supply a licensed font.ttf\n"
                "in that folder. Use the existing Homebrew Menu to open the NRO.\n\n"
                f"Complete corresponding source is supplied beside this ZIP as {SOURCE_NAME}.\n"
                "See SOURCE-SHA256 and RELEASE-IDENTITY.json for the exact release inputs.\n"
                "This software is based in part on the work of the Independent JPEG Group.\n"
            ).encode("utf-8"),
        }
        binary_sums = entries_checksums(binary)
        if binary_sums[NRO_INSTALL_PATH] != identity["nro_sha256"]:
            raise ValueError("The NRO changed during packaging")
        binary_archive = stage / BINARY_NAME
        write_zip(binary_archive, binary)
        verify_zip(binary_archive, binary_sums)
        (stage / "SHA256SUMS").write_bytes(checksum_text({
            BINARY_NAME: sha256(binary_archive),
            SOURCE_NAME: source_sha,
        }).encode("utf-8"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app-source", required=True, type=Path, help="reviewed, staged source tree")
    parser.add_argument("--dependencies", required=True, type=Path)
    parser.add_argument("--main-sha", required=True)
    parser.add_argument("--nro", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    try:
        bundle(args.app_source, args.dependencies, args.main_sha, args.nro, args.output_dir)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError,
            tarfile.TarError, zipfile.BadZipFile) as error:
        parser.exit(1, f"{error}\n")
    print(f"Verified {BINARY_NAME}, {SOURCE_NAME}, and SHA256SUMS in {args.output_dir}")


if __name__ == "__main__":
    main()

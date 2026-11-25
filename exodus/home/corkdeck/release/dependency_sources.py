#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Materialize and verify the pinned static dependency sources."""

import concurrent.futures
import contextlib
import hashlib
from itertools import takewhile
import json
import re
import shutil
import tarfile
import tempfile
import urllib.parse
import urllib.request
from pathlib import Path, PurePosixPath


MANIFEST = Path(__file__).with_name("dependency-sources.json")
SHA256 = re.compile(r"[0-9a-f]{64}")
CHUNK_SIZE = 1024 * 1024


def sha256(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def relative_path(value):
    path = PurePosixPath(value)
    if (
        path.is_absolute()
        or not path.parts
        or path.as_posix() != value
        or ".." in path.parts
        or any(":" in part or "\\" in part or any(ord(char) < 32 for char in part)
               for part in path.parts)
    ):
        raise ValueError(f"Invalid relative path: {value!r}")
    return path


def load_manifest():
    with MANIFEST.open(encoding="utf-8") as stream:
        manifest = json.load(stream)
    if manifest["format"] != 1:
        raise ValueError("Unknown dependency manifest format")
    entries = (manifest["inputs"] + manifest["notices"] + manifest["source_variants"]
               + manifest["external_recipe_references"])
    for entry in entries:
        relative_path(entry["path"])
        if not SHA256.fullmatch(entry["sha256"]) or entry["size"] <= 0:
            raise ValueError(f"Invalid size or digest for {entry['path']}")
    return manifest


def check_file(path, entry):
    if path.is_symlink() or not path.is_file():
        raise ValueError(f"Missing regular file: {path}")
    if path.stat().st_size != entry["size"] or sha256(path) != entry["sha256"]:
        raise ValueError(f"Size or SHA-256 mismatch: {path}")


def _is_link_or_reparse(path):
    if path.is_symlink():
        return True
    try:
        return bool(getattr(path.lstat(), "st_reparse_tag", 0))
    except OSError:
        return False


def list_files(root):
    if _is_link_or_reparse(root) or not root.is_dir():
        raise ValueError(f"Expected a directory: {root}")
    files = {}
    seen = set()
    # Path.rglob follows Windows directory junctions even with ordinary symlink
    # traversal disabled, so inspect each entry before adding a directory.
    pending = [root]
    while pending:
        directory = pending.pop()
        subdirectories = []
        for path in sorted(directory.iterdir()):
            name = relative_path(path.relative_to(root).as_posix())
            folded = name.as_posix().casefold()
            if folded in seen:
                raise ValueError(f"Case-insensitive path collision: {name}")
            seen.add(folded)
            if _is_link_or_reparse(path):
                raise ValueError(f"Unsupported file type: {path}")
            if path.is_file():
                files[name.as_posix()] = path
            elif path.is_dir():
                subdirectories.append(path)
            else:
                raise ValueError(f"Unsupported file type: {path}")
        pending.extend(reversed(subdirectories))
    return dict(sorted(files.items()))


@contextlib.contextmanager
def staged_output(output):
    output.parent.mkdir(parents=True, exist_ok=True)
    if output.exists() and (output.is_symlink() or not output.is_dir() or any(output.iterdir())):
        raise ValueError(f"Output directory must be empty: {output}")
    with tempfile.TemporaryDirectory(prefix=".exodus-source-", dir=output.parent) as temp:
        stage = Path(temp) / "ready"
        stage.mkdir()
        yield stage
        if output.exists():
            output.rmdir()
        stage.rename(output)


def cached_file(entry, cache, offline):
    cached = cache / entry["sha256"]
    if cached.exists():
        check_file(cached, entry)
        return cached
    if offline:
        raise ValueError(f"Source missing from offline cache: {entry['path']}")

    failures = []
    for url in entry["urls"]:
        if urllib.parse.urlsplit(url).scheme != "https":
            raise ValueError(f"Expected HTTPS source: {url}")
        with tempfile.NamedTemporaryFile(dir=cache, delete=False) as tmp:
            temporary = Path(tmp.name)
        try:
            request = urllib.request.Request(url, headers={"User-Agent": "EXODUS-source-bundle"})
            with urllib.request.urlopen(request, timeout=30) as response, temporary.open("wb") as target:
                if urllib.parse.urlsplit(response.geturl()).scheme != "https":
                    raise ValueError("Insecure source redirect")
                remaining = entry["size"]
                while chunk := response.read(min(CHUNK_SIZE, remaining + 1)):
                    remaining -= len(chunk)
                    if remaining < 0:
                        raise ValueError("Source exceeded its pinned size")
                    target.write(chunk)
            check_file(temporary, entry)
            temporary.replace(cached)
            return cached
        except (OSError, ValueError) as error:
            failures.append(f"{url}: {error}")
        finally:
            temporary.unlink(missing_ok=True)
    raise ValueError(f"Could not fetch {entry['path']}\n" + "\n".join(failures))


def c_license_comment(content):
    match = re.match(rb"/\*.*?\*/\r?\n", content, re.DOTALL)
    if (not match or b"Copyright" not in match[0]
            or not any(word in match[0] for word in (b"Permission", b"Redistribution"))):
        raise ValueError("Expected a complete leading C copyright and license notice")
    return match[0]


def patch_license_notice(source, member_name):
    marker = b"+++ b/" + member_name.encode("ascii")
    lines = source.read_bytes().splitlines()
    start = lines.index(marker) + 1
    if not lines[start].startswith(b"@@ -0,0 "):
        raise ValueError(f"Expected an added source file in {source}: {member_name}")
    additions = takewhile(lambda line: line.startswith(b"+"), lines[start + 1:])
    content = b"\n".join(line[1:] for line in additions) + b"\n"
    if content.startswith(b"/*"):
        return c_license_comment(content)
    header = re.match(rb"(?:(?:#[^\n]*)?\n)+", content)
    if not header or b"Copyright" not in header[0] or b"Permission" not in header[0]:
        raise ValueError(f"Expected a leading build-file license notice: {member_name}")
    return header[0].rstrip() + b"\n"


def extract_notices(stage, notices, sources):
    grouped = {}
    for notice in notices:
        if notice.get("patch_member"):
            destination = stage / notice["path"]
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(patch_license_notice(sources[notice["source"]], notice["member"]))
            continue
        grouped.setdefault(notice["source"], {})[notice["member"]] = notice
    for source, expected in grouped.items():
        with tarfile.open(sources[source], "r|*") as archive:
            for member in archive:
                notice = expected.pop(member.name, None)
                if notice is None:
                    continue
                if not member.isfile():
                    raise ValueError(f"Expected a regular notice in {source}: {member.name}")
                destination = stage / notice["path"]
                destination.parent.mkdir(parents=True, exist_ok=True)
                with archive.extractfile(member) as content, destination.open("wb") as target:
                    if notice.get("leading_comment"):
                        target.write(c_license_comment(content.read()))
                    else:
                        shutil.copyfileobj(content, target)
                if not expected:
                    break
        if expected:
            raise ValueError(f"Missing notices in {source}: {sorted(expected)}")


def source_variant(source, destination, omitted):
    removed = set()
    with tarfile.open(source, "r:*") as original, tarfile.open(destination, "w") as result:
        for member in original:
            relative_path(member.name)
            if member.name in omitted:
                removed.add(member.name)
                continue
            if not (member.isfile() or member.isdir()):
                raise ValueError(f"Unsupported member in {source}: {member.name}")
            member.uid = member.gid = member.mtime = 0
            member.uname = member.gname = ""
            member.pax_headers = {}
            if member.isfile():
                with original.extractfile(member) as stream:
                    result.addfile(member, stream)
            else:
                result.addfile(member)
    if removed != set(omitted):
        raise ValueError(f"Unexpected omitted members in {source}")


def staged_entries(manifest):
    originals_in_cache = {entry["source"] for entry in manifest["source_variants"]}
    return [
        entry for entry in manifest["inputs"] if entry["path"] not in originals_in_cache
    ] + manifest["source_variants"] + manifest["notices"]


def checksum_text(files):
    return "".join(f"{digest}  {name}\n" for name, digest in sorted(files.items()))


def dependency_checksums(manifest):
    entries = {entry["path"]: entry["sha256"] for entry in staged_entries(manifest)}
    entries[MANIFEST.name] = sha256(MANIFEST)
    return checksum_text(entries)


def verify_dependencies(root, manifest):
    files = list_files(root)
    entries = staged_entries(manifest)
    expected = {entry["path"] for entry in entries} | {MANIFEST.name, "SHA256SUMS"}
    if set(files) != expected:
        difference = sorted(set(files) ^ expected)
        raise ValueError(f"Dependency bundle file set differs from the manifest: {difference}")
    for entry in entries:
        check_file(files[entry["path"]], entry)
    if files[MANIFEST.name].read_bytes() != MANIFEST.read_bytes():
        raise ValueError("Dependency bundle was staged from a different manifest")
    if files["SHA256SUMS"].read_bytes() != dependency_checksums(manifest).encode("utf-8"):
        raise ValueError("Dependency bundle checksums do not match")
    return files


def fetch_dependencies(cache, output, offline=False):
    manifest = load_manifest()
    cache.mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
        inputs = manifest["inputs"]
        cached = pool.map(lambda entry: cached_file(entry, cache, offline), inputs)
        sources = {entry["path"]: path for entry, path in zip(inputs, cached, strict=True)}
    with staged_output(output) as stage:
        variants = {entry["path"]: entry for entry in manifest["source_variants"]}
        extract_notices(stage, manifest["notices"], sources)
        for entry in staged_entries(manifest):
            name = entry["path"]
            if name not in sources and name not in variants:
                continue
            destination = stage / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            if name in variants:
                source_variant(sources[entry["source"]], destination, entry["omit"])
            else:
                shutil.copyfile(sources[name], destination)
        shutil.copyfile(MANIFEST, stage / MANIFEST.name)
        (stage / "SHA256SUMS").write_bytes(dependency_checksums(manifest).encode("utf-8"))
        verify_dependencies(stage, manifest)

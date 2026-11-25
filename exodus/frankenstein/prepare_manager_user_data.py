#!/usr/bin/env python3
"""Embed the pinned FreeBSD input port patch in manager cloud-init data."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import tempfile


MARKER = "__EXODUS_MANAGER_INPUT_PATCH__"
HEREDOC_END = "EXODUS_INPUT_PATCH"
MAX_TEMPLATE_BYTES = 64 * 1024
MAX_PATCH_BYTES = 128 * 1024


class PreparationError(ValueError):
    """The template or port patch is not safe to embed."""


def _read_utf8(path: Path, limit: int, label: str) -> str:
    payload = path.read_bytes()
    if not payload or len(payload) > limit:
        raise PreparationError(f"{label} must contain 1..{limit} bytes")
    try:
        text = payload.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise PreparationError(f"{label} must be UTF-8") from exc
    if "\r" in text or "\0" in text:
        raise PreparationError(f"{label} must use plain LF text")
    return text


def render(template_path: Path, patch_path: Path) -> bytes:
    template = _read_utf8(template_path, MAX_TEMPLATE_BYTES, "manager template")
    patch = _read_utf8(patch_path, MAX_PATCH_BYTES, "manager input patch")
    if template.count(MARKER) != 1:
        raise PreparationError("manager template must contain exactly one patch marker")
    if any(line == HEREDOC_END for line in patch.splitlines()):
        raise PreparationError("manager input patch terminates its embedding heredoc")
    expected_diffs = (
        "diff --git a/input-linux-0.7.1/src/evdev.rs b/input-linux-0.7.1/src/evdev.rs",
        "diff --git a/input-linux-0.7.1/src/macros.rs b/input-linux-0.7.1/src/macros.rs",
        "diff --git a/input-linux-0.7.1/src/uinput.rs b/input-linux-0.7.1/src/uinput.rs",
        "diff --git a/input-linux-sys-0.9.0/src/freebsd.rs b/input-linux-sys-0.9.0/src/freebsd.rs",
        "diff --git a/input-linux-sys-0.9.0/src/input.rs b/input-linux-sys-0.9.0/src/input.rs",
        "diff --git a/input-linux-sys-0.9.0/src/uinput.rs b/input-linux-sys-0.9.0/src/uinput.rs",
    )
    if tuple(line for line in patch.splitlines() if line.startswith("diff --git ")) != expected_diffs:
        raise PreparationError("manager input patch changes unexpected crate paths")
    rendered = template.replace(MARKER, patch.rstrip("\n"))
    return rendered.encode("utf-8")


def write_atomic(path: Path, payload: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=f".{path.name}.", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(payload)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("template", type=Path)
    parser.add_argument("patch", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args(argv)
    write_atomic(args.output, render(args.template, args.patch))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Embed pinned FreeBSD input and platform patches in manager cloud-init data."""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import tempfile


INPUT_MARKER = "__EXODUS_MANAGER_INPUT_PATCH__"
INPUT_HEREDOC_END = "EXODUS_INPUT_PATCH"
PLATFORM_MARKER = "__EXODUS_MANAGER_PLATFORM_PATCH__"
PLATFORM_HEREDOC_END = "EXODUS_PLATFORM_PATCH"
# Backwards-compatible names retained for callers that validate the input patch.
MARKER = INPUT_MARKER
HEREDOC_END = INPUT_HEREDOC_END
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


def _validate_patch_paths(patch: str, expected: tuple[str, ...], label: str) -> None:
    actual = tuple(line for line in patch.splitlines() if line.startswith("diff --git "))
    expected_headers = tuple(f"diff --git a/{path} b/{path}" for path in expected)
    if actual != expected_headers:
        raise PreparationError(f"{label} changes unexpected paths")


def render(template_path: Path, input_patch_path: Path, platform_patch_path: Path) -> bytes:
    template = _read_utf8(template_path, MAX_TEMPLATE_BYTES, "manager template")
    input_patch = _read_utf8(input_patch_path, MAX_PATCH_BYTES, "manager input patch")
    platform_patch = _read_utf8(platform_patch_path, MAX_PATCH_BYTES, "manager platform patch")
    if template.count(INPUT_MARKER) != 1:
        raise PreparationError("manager template must contain exactly one input patch marker")
    if template.count(PLATFORM_MARKER) != 1:
        raise PreparationError("manager template must contain exactly one platform patch marker")
    for marker in (INPUT_MARKER, PLATFORM_MARKER):
        if marker in input_patch or marker in platform_patch:
            raise PreparationError("manager patch payload contains an embedding marker")
    if any(line == INPUT_HEREDOC_END for line in input_patch.splitlines()):
        raise PreparationError("manager input patch terminates its embedding heredoc")
    if any(line == PLATFORM_HEREDOC_END for line in platform_patch.splitlines()):
        raise PreparationError("manager platform patch terminates its embedding heredoc")
    expected_input_diffs = (
        "diff --git a/input-linux-0.7.1/src/evdev.rs b/input-linux-0.7.1/src/evdev.rs",
        "diff --git a/input-linux-0.7.1/src/macros.rs b/input-linux-0.7.1/src/macros.rs",
        "diff --git a/input-linux-0.7.1/src/uinput.rs b/input-linux-0.7.1/src/uinput.rs",
        "diff --git a/input-linux-sys-0.9.0/src/freebsd.rs b/input-linux-sys-0.9.0/src/freebsd.rs",
        "diff --git a/input-linux-sys-0.9.0/src/input.rs b/input-linux-sys-0.9.0/src/input.rs",
        "diff --git a/input-linux-sys-0.9.0/src/uinput.rs b/input-linux-sys-0.9.0/src/uinput.rs",
    )
    if tuple(line for line in input_patch.splitlines() if line.startswith("diff --git ")) != expected_input_diffs:
        raise PreparationError("manager input patch changes unexpected crate paths")
    _validate_patch_paths(
        platform_patch,
        (
            "Cargo.lock",
            "steamos-manager/Cargo.toml",
            "steamos-manager/examples/freebsd_inotify_probe.rs",
            "steamos-manager/src/cec.rs",
            "steamos-manager/src/cec_types.rs",
            "steamos-manager/src/hardware.rs",
            "steamos-manager/src/lib.rs",
            "steamos-manager/src/manager/user.rs",
        ),
        "manager platform patch",
    )
    rendered = template.replace(INPUT_MARKER, input_patch.rstrip("\n"))
    rendered = rendered.replace(PLATFORM_MARKER, platform_patch.rstrip("\n"))
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
    parser.add_argument("input_patch", type=Path)
    parser.add_argument("platform_patch", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args(argv)
    write_atomic(args.output, render(args.template, args.input_patch, args.platform_patch))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

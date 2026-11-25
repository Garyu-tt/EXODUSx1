"""Bounded offline admission for EXODUS-owned Horizon package manifests.

This module validates metadata supplied by a caller.  It does not read package
archives, verify signatures, install files, execute entrypoints, or grant a
capability.  A valid manifest is only an input to later trusted stages.
"""

from __future__ import annotations

from dataclasses import dataclass
from hashlib import sha256
import json
import re
import unicodedata
from typing import Final
from urllib.parse import urlsplit


SCHEMA: Final = "exodus.xpkg.horizon-manifest/1"
ARCHITECTURE: Final = "aarch64"
ABI: Final = "horizon-nro"
ABI_VERSION: Final = 1

MAX_MANIFEST_BYTES: Final = 64 * 1024
MAX_JSON_DEPTH: Final = 8
MAX_OBJECT_FIELDS: Final = 32
MAX_KEY_CHARACTERS: Final = 64
MAX_FILES: Final = 256
MAX_REQUIREMENTS: Final = 128
MAX_CAPABILITIES: Final = 32
MAX_PATH_BYTES: Final = 240
MAX_COMPONENT_BYTES: Final = 120
MAX_FILE_BYTES: Final = 16 * 1024**3
MAX_TOTAL_BYTES: Final = 64 * 1024**3
MAX_SOURCE_URL_CHARACTERS: Final = 2048

_PACKAGE_RE = re.compile(r"[a-z0-9](?:[a-z0-9._-]{0,62}[a-z0-9])?\Z")
_SPDX_RE = re.compile(r"[A-Za-z0-9][A-Za-z0-9.+-]{0,63}\Z")
_CAPABILITY_RE = re.compile(r"[a-z][a-z0-9_]{0,63}\Z")
_SEMVER_RE = re.compile(
    r"(?P<major>0|[1-9][0-9]{0,9})\."
    r"(?P<minor>0|[1-9][0-9]{0,9})\."
    r"(?P<patch>0|[1-9][0-9]{0,9})"
    r"(?:-(?P<prerelease>[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?"
    r"(?:\+(?P<build>[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?\Z"
)
_LOWER_SHA256_RE = re.compile(r"[0-9a-f]{64}\Z")
_BAD_PERCENT_ESCAPE_RE = re.compile(r"%(?![0-9A-Fa-f]{2})")
_RESERVED_COMPONENTS = {
    "con",
    "prn",
    "aux",
    "nul",
    "conin$",
    "conout$",
    *(f"com{number}" for number in range(1, 10)),
    *(f"lpt{number}" for number in range(1, 10)),
    *(f"com{number}" for number in "¹²³"),
    *(f"lpt{number}" for number in "¹²³"),
}

POLICY_ALLOWED: Final = frozenset({
    "read_local_status",
    "read_local_catalog",
})
PROVIDER_REVIEW_REQUIRED: Final = frozenset({
    "read_local_file",
    "ssh_client",
    "lan_service",
    "homebrew_launch",
    "native_title_launch",
})
POLICY_DENIED: Final = frozenset({
    "nintendo_eshop",
    "nintendo_account",
    "nintendo_online",
    "nintendo_service",
    "license_mutation",
    "system_content_install",
    "unreviewed_plugin",
    "raw_privileged_dispatch",
})


class ManifestError(ValueError):
    """The supplied manifest is not a valid bounded R01 contract."""


def _native_int(value: object, name: str, minimum: int, maximum: int) -> int:
    if type(value) is not int or not minimum <= value <= maximum:
        raise ManifestError(f"{name} must be a native integer in [{minimum}, {maximum}]")
    return value


def _text(value: object, name: str, maximum: int) -> str:
    if type(value) is not str or not value or len(value) > maximum:
        raise ManifestError(f"{name} must be nonempty text of at most {maximum} characters")
    return value


def _exact_dict(value: object, fields: frozenset[str], name: str) -> dict[str, object]:
    if type(value) is not dict or len(value) != len(fields) or frozenset(value) != fields:
        raise ManifestError(f"{name} must contain exactly {sorted(fields)}")
    return value


def _exact_list(value: object, name: str, maximum: int, *, minimum: int = 0) -> list[object]:
    if type(value) is not list or not minimum <= len(value) <= maximum:
        raise ManifestError(f"{name} must contain between {minimum} and {maximum} items")
    return value


def _package_name(value: object, name: str = "package name") -> str:
    text = _text(value, name, 64)
    if _PACKAGE_RE.fullmatch(text) is None:
        raise ManifestError(f"invalid {name}")
    return text


def _semantic_version(value: object, name: str) -> str:
    text = _text(value, name, 64)
    match = _SEMVER_RE.fullmatch(text)
    if match is None:
        raise ManifestError(f"{name} must be canonical SemVer")
    if any(int(match.group(part)) > 2**31 - 1 for part in ("major", "minor", "patch")):
        raise ManifestError(f"{name} numeric component is too large")
    prerelease = match.group("prerelease")
    if prerelease is not None:
        for identifier in prerelease.split("."):
            if identifier.isdigit() and len(identifier) > 1 and identifier.startswith("0"):
                raise ManifestError(f"{name} has a noncanonical numeric prerelease")
    return text


def _lower_sha256(value: object, name: str) -> str:
    text = _text(value, name, 64)
    if _LOWER_SHA256_RE.fullmatch(text) is None:
        raise ManifestError(f"{name} must be 64 lowercase hexadecimal characters")
    return text


def _ascii_fold(value: str) -> str:
    return "".join(chr(ord(ch) + 32) if "A" <= ch <= "Z" else ch for ch in value)


def _package_path(value: object, name: str) -> str:
    text = _text(value, name, MAX_PATH_BYTES)
    if unicodedata.normalize("NFC", text) != text:
        raise ManifestError(f"{name} must use NFC Unicode normalization")
    try:
        encoded = text.encode("utf-8")
    except UnicodeEncodeError as exc:
        raise ManifestError(f"{name} is not valid UTF-8 text") from exc
    if len(encoded) > MAX_PATH_BYTES or text.startswith("/") or "\\" in text or ":" in text:
        raise ManifestError(f"invalid relative {name}")
    components = text.split("/")
    if any(component in {"", ".", ".."} for component in components):
        raise ManifestError(f"invalid relative {name}")
    for component in components:
        if len(component.encode("utf-8")) > MAX_COMPONENT_BYTES:
            raise ManifestError(f"{name} component is too long")
        if component.endswith((" ", ".")):
            raise ManifestError(f"{name} is not portable to FAT-style storage")
        if any(ord(ch) < 0x20 or 0x7F <= ord(ch) <= 0x9F
               or ch in '<>"|?*' for ch in component):
            raise ManifestError(f"{name} contains a reserved character")
        stem = _ascii_fold(component).split(".", 1)[0].rstrip(" .")
        if stem in _RESERVED_COMPONENTS:
            raise ManifestError(f"{name} uses a reserved storage component")
    return text


def _source_url(value: object) -> str:
    text = _text(value, "source.url", MAX_SOURCE_URL_CHARACTERS)
    try:
        text.encode("ascii")
        parsed = urlsplit(text)
        port = parsed.port
    except (UnicodeEncodeError, ValueError) as exc:
        raise ManifestError("source.url must be a valid ASCII HTTPS URL") from exc
    # urlsplit is a splitter, not an authority grammar validator. Require
    # an exact host[:port] or bracketed literal[:port] shape.
    authority = parsed.netloc
    if re.fullmatch(r"(?:[A-Za-z0-9.-]+|\[[0-9A-Fa-f:.]+\])(?::[0-9]+)?", authority) is None:
        raise ManifestError("source.url has an invalid HTTPS authority")
    if (parsed.scheme != "https" or not parsed.hostname or parsed.username is not None
            or parsed.password is not None or "#" in text or "\\" in text
            or "%" in parsed.netloc or parsed.netloc.endswith(":")
            or _BAD_PERCENT_ESCAPE_RE.search(text) is not None
            or port is not None and not 1 <= port <= 65535
            or any(ch.isspace() or ord(ch) < 0x20 or ord(ch) == 0x7f for ch in text)):
        raise ManifestError("source.url must be an unambiguous credential-free HTTPS URL")
    return text


@dataclass(frozen=True, slots=True)
class Target:
    architecture: str
    abi: str
    abi_version: int

    def __post_init__(self) -> None:
        if type(self.architecture) is not str or self.architecture != ARCHITECTURE:
            raise ManifestError(f"target.architecture must be {ARCHITECTURE}")
        if type(self.abi) is not str or self.abi != ABI:
            raise ManifestError(f"target.abi must be {ABI}")
        _native_int(self.abi_version, "target.abi_version", ABI_VERSION, ABI_VERSION)


@dataclass(frozen=True, slots=True)
class FileRecord:
    path: str
    size: int
    sha256: str

    def __post_init__(self) -> None:
        _package_path(self.path, "file path")
        _native_int(self.size, "file size", 0, MAX_FILE_BYTES)
        _lower_sha256(self.sha256, "file sha256")


@dataclass(frozen=True, slots=True)
class Requirement:
    name: str
    version: str

    def __post_init__(self) -> None:
        _package_name(self.name, "requirement name")
        _semantic_version(self.version, "requirement version")


@dataclass(frozen=True, slots=True)
class SourceRecord:
    url: str
    sha256: str

    def __post_init__(self) -> None:
        _source_url(self.url)
        _lower_sha256(self.sha256, "source.sha256")


def _check_files(files: tuple[FileRecord, ...]) -> None:
    if type(files) is not tuple or not 1 <= len(files) <= MAX_FILES:
        raise ManifestError(f"files must contain between 1 and {MAX_FILES} records")
    if any(type(record) is not FileRecord for record in files):
        raise ManifestError("files must contain exact FileRecord values")
    expected = tuple(sorted(files, key=lambda record: _ascii_fold(record.path)))
    if files != expected:
        raise ManifestError("files must use canonical path order")
    folded = [_ascii_fold(record.path) for record in files]
    if len(folded) != len(set(folded)):
        raise ManifestError("file paths collide under ASCII case folding")
    folded_set = set(folded)
    for current in folded:
        components = current.split("/")
        if any("/".join(components[:end]) in folded_set for end in range(1, len(components))):
            raise ManifestError("a file path cannot also be a directory prefix")
    total = sum(record.size for record in files)
    if total > MAX_TOTAL_BYTES:
        raise ManifestError("manifest file total exceeds the R01 byte budget")


def _check_requirements(
    values: tuple[Requirement, ...], name: str
) -> None:
    if type(values) is not tuple or len(values) > MAX_REQUIREMENTS:
        raise ManifestError(f"{name} exceeds the R01 requirement limit")
    if any(type(value) is not Requirement for value in values):
        raise ManifestError(f"{name} must contain exact Requirement values")
    expected = tuple(sorted(values, key=lambda value: value.name))
    if values != expected or len({value.name for value in values}) != len(values):
        raise ManifestError(f"{name} must have unique names in canonical order")


@dataclass(frozen=True, slots=True)
class HorizonManifest:
    schema: str
    name: str
    version: str
    target: Target
    entrypoint: str
    files: tuple[FileRecord, ...]
    dependencies: tuple[Requirement, ...]
    conflicts: tuple[Requirement, ...]
    license: str
    source: SourceRecord
    capabilities: tuple[str, ...]

    def __post_init__(self) -> None:
        if type(self.schema) is not str or self.schema != SCHEMA:
            raise ManifestError("unsupported xpkg manifest schema")
        _package_name(self.name)
        _semantic_version(self.version, "package version")
        if type(self.target) is not Target:
            raise ManifestError("target must be an exact Target")
        entrypoint = _package_path(self.entrypoint, "entrypoint")
        _check_files(self.files)
        matching = [record for record in self.files if record.path == entrypoint]
        if len(matching) != 1 or matching[0].size == 0 or not entrypoint.lower().endswith(".nro"):
            raise ManifestError("entrypoint must name one nonempty NRO file record")
        _check_requirements(self.dependencies, "dependencies")
        _check_requirements(self.conflicts, "conflicts")
        dependency_names = {value.name for value in self.dependencies}
        conflict_names = {value.name for value in self.conflicts}
        if self.name in dependency_names or self.name in conflict_names:
            raise ManifestError("a package cannot depend on or conflict with itself")
        if dependency_names & conflict_names:
            raise ManifestError("a package name cannot be both a dependency and a conflict")
        license_text = _text(self.license, "license", 64)
        if _SPDX_RE.fullmatch(license_text) is None:
            raise ManifestError("license must be one SPDX identifier")
        if type(self.source) is not SourceRecord:
            raise ManifestError("source must be an exact SourceRecord")
        if type(self.capabilities) is not tuple or len(self.capabilities) > MAX_CAPABILITIES:
            raise ManifestError("capabilities exceeds the R01 limit")
        if self.capabilities != tuple(sorted(self.capabilities)) or len(set(self.capabilities)) != len(self.capabilities):
            raise ManifestError("capabilities must be unique and in canonical order")
        for capability in self.capabilities:
            if type(capability) is not str or _CAPABILITY_RE.fullmatch(capability) is None:
                raise ManifestError("invalid capability identifier")


@dataclass(frozen=True, slots=True)
class CapabilityAdmission:
    disposition: str
    allowed: tuple[str, ...]
    review_required: tuple[str, ...]
    denied: tuple[str, ...]
    unknown: tuple[str, ...]


@dataclass(frozen=True, slots=True)
class ManifestInspection:
    manifest: HorizonManifest
    canonical_sha256: str
    capability_admission: CapabilityAdmission


def _reject_float(_: str) -> object:
    raise ManifestError("floating-point JSON numbers are not allowed")


def _reject_constant(_: str) -> object:
    raise ManifestError("nonstandard JSON constants are not allowed")


def _pairs_to_dict(pairs: list[tuple[str, object]]) -> dict[str, object]:
    if len(pairs) > MAX_OBJECT_FIELDS:
        raise ManifestError("JSON object has too many fields")
    result: dict[str, object] = {}
    for key, value in pairs:
        if type(key) is not str or not key or len(key) > MAX_KEY_CHARACTERS:
            raise ManifestError("invalid JSON object key")
        if key in result:
            raise ManifestError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def _check_json_depth(text: str) -> None:
    quoted = False
    escaped = False
    depth = 0
    for character in text:
        if quoted:
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == '"':
                quoted = False
        elif character == '"':
            quoted = True
        elif character in "[{":
            depth += 1
            if depth > MAX_JSON_DEPTH:
                raise ManifestError("JSON nesting exceeds the R01 depth limit")
        elif character in "]}":
            depth -= 1
            if depth < 0:
                raise ManifestError("invalid JSON structure")


def _load_json(data: object) -> dict[str, object]:
    if type(data) is not bytes:
        raise ManifestError("manifest input must be exact bytes")
    if not data or len(data) > MAX_MANIFEST_BYTES:
        raise ManifestError(f"manifest input must contain 1 to {MAX_MANIFEST_BYTES} bytes")
    if data.startswith((b"\xef\xbb\xbf", b"\xff\xfe", b"\xfe\xff")):
        raise ManifestError("manifest input must be UTF-8 without a byte-order mark")
    try:
        text = data.decode("utf-8", "strict")
        _check_json_depth(text)
        value = json.loads(
            text,
            object_pairs_hook=_pairs_to_dict,
            parse_float=_reject_float,
            parse_constant=_reject_constant,
        )
    except ManifestError:
        raise
    except (UnicodeDecodeError, json.JSONDecodeError, RecursionError, ValueError) as exc:
        raise ManifestError("invalid manifest JSON") from exc
    if type(value) is not dict:
        raise ManifestError("manifest root must be an object")
    return value


def _parse_target(value: object) -> Target:
    raw = _exact_dict(value, frozenset({"architecture", "abi", "abi_version"}), "target")
    return Target(raw["architecture"], raw["abi"], raw["abi_version"])


def _parse_files(value: object) -> tuple[FileRecord, ...]:
    raw_files = _exact_list(value, "files", MAX_FILES, minimum=1)
    files = []
    fields = frozenset({"path", "size", "sha256"})
    for index, value in enumerate(raw_files):
        raw = _exact_dict(value, fields, f"files[{index}]")
        files.append(FileRecord(raw["path"], raw["size"], raw["sha256"]))
    return tuple(sorted(files, key=lambda record: _ascii_fold(record.path)))


def _parse_requirements(value: object, name: str) -> tuple[Requirement, ...]:
    raw_values = _exact_list(value, name, MAX_REQUIREMENTS)
    requirements = []
    fields = frozenset({"name", "version"})
    for index, value in enumerate(raw_values):
        raw = _exact_dict(value, fields, f"{name}[{index}]")
        requirements.append(Requirement(raw["name"], raw["version"]))
    return tuple(sorted(requirements, key=lambda requirement: requirement.name))


def _parse_source(value: object) -> SourceRecord:
    raw = _exact_dict(value, frozenset({"url", "sha256"}), "source")
    return SourceRecord(raw["url"], raw["sha256"])


def _parse_capabilities(value: object) -> tuple[str, ...]:
    raw_values = _exact_list(value, "capabilities", MAX_CAPABILITIES)
    capabilities = []
    for value in raw_values:
        if type(value) is not str or _CAPABILITY_RE.fullmatch(value) is None:
            raise ManifestError("invalid capability identifier")
        capabilities.append(value)
    return tuple(sorted(capabilities))


def parse_manifest(data: bytes) -> HorizonManifest:
    """Parse and normalize one bounded manifest without performing I/O."""
    fields = frozenset({
        "schema",
        "name",
        "version",
        "target",
        "entrypoint",
        "files",
        "dependencies",
        "conflicts",
        "license",
        "source",
        "capabilities",
    })
    raw = _exact_dict(_load_json(data), fields, "manifest")
    return HorizonManifest(
        schema=raw["schema"],
        name=raw["name"],
        version=raw["version"],
        target=_parse_target(raw["target"]),
        entrypoint=raw["entrypoint"],
        files=_parse_files(raw["files"]),
        dependencies=_parse_requirements(raw["dependencies"], "dependencies"),
        conflicts=_parse_requirements(raw["conflicts"], "conflicts"),
        license=raw["license"],
        source=_parse_source(raw["source"]),
        capabilities=_parse_capabilities(raw["capabilities"]),
    )


def _checked_manifest(manifest: object) -> HorizonManifest:
    if type(manifest) is not HorizonManifest:
        raise ManifestError("an exact HorizonManifest is required")
    return manifest


def manifest_to_dict(manifest: HorizonManifest) -> dict[str, object]:
    """Return a detached normalized wire representation."""
    value = _checked_manifest(manifest)
    return {
        "schema": value.schema,
        "name": value.name,
        "version": value.version,
        "target": {
            "architecture": value.target.architecture,
            "abi": value.target.abi,
            "abi_version": value.target.abi_version,
        },
        "entrypoint": value.entrypoint,
        "files": [
            {"path": record.path, "size": record.size, "sha256": record.sha256}
            for record in value.files
        ],
        "dependencies": [
            {"name": requirement.name, "version": requirement.version}
            for requirement in value.dependencies
        ],
        "conflicts": [
            {"name": requirement.name, "version": requirement.version}
            for requirement in value.conflicts
        ],
        "license": value.license,
        "source": {"url": value.source.url, "sha256": value.source.sha256},
        "capabilities": list(value.capabilities),
    }


def canonical_bytes(manifest: HorizonManifest) -> bytes:
    """Return the canonical UTF-8 consistency representation."""
    return json.dumps(
        manifest_to_dict(manifest),
        sort_keys=True,
        separators=(",", ":"),
        ensure_ascii=False,
        allow_nan=False,
    ).encode("utf-8")


def manifest_digest(manifest: HorizonManifest) -> str:
    """Return a consistency digest; this is not a signature."""
    return sha256(canonical_bytes(manifest)).hexdigest()


def classify_capabilities(manifest: HorizonManifest) -> CapabilityAdmission:
    """Classify declarations without granting install or execution authority."""
    value = _checked_manifest(manifest)
    allowed = tuple(cap for cap in value.capabilities if cap in POLICY_ALLOWED)
    review = tuple(cap for cap in value.capabilities if cap in PROVIDER_REVIEW_REQUIRED)
    denied = tuple(cap for cap in value.capabilities if cap in POLICY_DENIED)
    known = POLICY_ALLOWED | PROVIDER_REVIEW_REQUIRED | POLICY_DENIED
    unknown = tuple(cap for cap in value.capabilities if cap not in known)
    if denied or unknown:
        disposition = "denied"
    elif review:
        disposition = "provider_review_required"
    else:
        disposition = "policy_allowed"
    return CapabilityAdmission(disposition, allowed, review, denied, unknown)


def inspect_manifest(data: bytes) -> ManifestInspection:
    """Parse, digest, and classify a manifest without authorizing activation."""
    manifest = parse_manifest(data)
    return ManifestInspection(
        manifest=manifest,
        canonical_sha256=manifest_digest(manifest),
        capability_admission=classify_capabilities(manifest),
    )

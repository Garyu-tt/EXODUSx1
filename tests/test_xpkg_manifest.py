"""Offline acceptance tests for the bounded xpkg Horizon manifest contract."""

import copy
from dataclasses import FrozenInstanceError, replace
import json
from pathlib import Path
import re
import unittest

from exodus import xpkg_manifest as xm


ZERO_DIGEST = "0" * 64
ONE_DIGEST = "1" * 64
CANONICAL_MANIFEST_DIGEST = "e12f0141f0f2bc17c7a20d2d05daf6d0a310682a2ecf897ea1e7c0cd5068c9be"


def manifest_object():
    return {
        "schema": xm.SCHEMA,
        "name": "example-tool",
        "version": "1.2.3",
        "target": {
            "architecture": xm.ARCHITECTURE,
            "abi": xm.ABI,
            "abi_version": xm.ABI_VERSION,
        },
        "entrypoint": "bin/example-tool.nro",
        "files": [
            {
                "path": "bin/example-tool.nro",
                "size": 4096,
                "sha256": ZERO_DIGEST,
            },
            {
                "path": "share/example-tool/readme.txt",
                "size": 72,
                "sha256": ONE_DIGEST,
            },
        ],
        "dependencies": [
            {"name": "runtime-assets", "version": "2.0.0"},
        ],
        "conflicts": [
            {"name": "example-tool-legacy", "version": "0.9.0"},
        ],
        "license": "MIT",
        "source": {
            "url": "https://example.invalid/releases/example-tool-1.2.3.tar.gz",
            "sha256": ONE_DIGEST,
        },
        "capabilities": ["read_local_catalog", "read_local_status"],
    }


def encode(value):
    return json.dumps(value, ensure_ascii=False, separators=(",", ":")).encode("utf-8")


class ManifestParsingTests(unittest.TestCase):
    def test_valid_manifest_is_normalized_and_immutable(self):
        value = manifest_object()
        value["files"].reverse()
        value["capabilities"].reverse()
        manifest = xm.parse_manifest(encode(value))
        self.assertEqual(manifest.name, "example-tool")
        self.assertEqual(
            [record.path for record in manifest.files],
            ["bin/example-tool.nro", "share/example-tool/readme.txt"],
        )
        self.assertEqual(
            manifest.capabilities,
            ("read_local_catalog", "read_local_status"),
        )
        with self.assertRaises((FrozenInstanceError, AttributeError)):
            manifest.name = "changed"

    def test_canonical_digest_ignores_json_layout_and_list_order(self):
        first = manifest_object()
        second = copy.deepcopy(first)
        second["files"].reverse()
        second["dependencies"].reverse()
        second["capabilities"].reverse()
        a = xm.parse_manifest(json.dumps(first, indent=2).encode())
        b = xm.parse_manifest(encode(second))
        self.assertEqual(a, b)
        self.assertEqual(xm.canonical_bytes(a), xm.canonical_bytes(b))
        self.assertEqual(xm.manifest_digest(a), xm.manifest_digest(b))
        self.assertEqual(xm.manifest_digest(a), CANONICAL_MANIFEST_DIGEST)

    def test_wire_copy_is_detached(self):
        manifest = xm.parse_manifest(encode(manifest_object()))
        wire = xm.manifest_to_dict(manifest)
        wire["files"][0]["path"] = "changed.nro"
        wire["target"]["abi_version"] = 99
        wire["capabilities"].append("nintendo_eshop")
        self.assertEqual(manifest.entrypoint, "bin/example-tool.nro")
        self.assertEqual(manifest.target.abi_version, 1)
        self.assertNotIn("nintendo_eshop", manifest.capabilities)

    def test_input_must_be_bounded_exact_utf8_bytes_without_bom(self):
        valid = encode(manifest_object())
        for value in (
            valid.decode(),
            bytearray(valid),
            b"",
            b"\xef\xbb\xbf" + valid,
            b"\xff\xfe{}",
            b"\xff",
            b" " * (xm.MAX_MANIFEST_BYTES + 1),
        ):
            with self.subTest(value_type=type(value), length=len(value)), self.assertRaises(
                xm.ManifestError
            ):
                xm.parse_manifest(value)

    def test_duplicate_keys_depth_floats_and_constants_fail_closed(self):
        duplicate = encode(manifest_object()).replace(
            b'"name":"example-tool"',
            b'"name":"example-tool","name":"shadow"',
            1,
        )
        for payload in (
            duplicate,
            b"[" * (xm.MAX_JSON_DEPTH + 1) + b"]" * (xm.MAX_JSON_DEPTH + 1),
            b'{"value":1.5}',
            b'{"value":NaN}',
            b'{"value":Infinity}',
        ):
            with self.subTest(payload=payload[:80]), self.assertRaises(xm.ManifestError):
                xm.parse_manifest(payload)

    def test_root_and_each_object_require_exact_fields(self):
        mutations = []
        extra_root = manifest_object()
        extra_root["extra"] = None
        mutations.append(extra_root)
        missing_root = manifest_object()
        missing_root.pop("license")
        mutations.append(missing_root)
        extra_target = manifest_object()
        extra_target["target"]["runtime"] = "freebsd"
        mutations.append(extra_target)
        missing_file = manifest_object()
        missing_file["files"][0].pop("sha256")
        mutations.append(missing_file)
        extra_source = manifest_object()
        extra_source["source"]["signature"] = "not-admitted-in-r01"
        mutations.append(extra_source)
        for value in mutations:
            with self.subTest(value=value), self.assertRaises(xm.ManifestError):
                xm.parse_manifest(encode(value))

    def test_native_integer_fields_reject_booleans_and_strings(self):
        for path, value in (
            (("target", "abi_version"), True),
            (("files", 0, "size"), False),
            (("files", 0, "size"), "4096"),
        ):
            candidate = manifest_object()
            current = candidate
            for key in path[:-1]:
                current = current[key]
            current[path[-1]] = value
            with self.subTest(path=path, value=value), self.assertRaises(xm.ManifestError):
                xm.parse_manifest(encode(candidate))


class IdentityAndTargetTests(unittest.TestCase):
    def test_package_names_are_lowercase_bounded_identifiers(self):
        for name in (
            "",
            "Example",
            "-example",
            "example-",
            "example/tool",
            "example tool",
            "éxample",
            "a" * 65,
        ):
            candidate = manifest_object()
            candidate["name"] = name
            with self.subTest(name=name), self.assertRaises(xm.ManifestError):
                xm.parse_manifest(encode(candidate))

    def test_versions_use_canonical_bounded_semver(self):
        valid = ("0.0.0", "1.2.3-alpha.1", "1.2.3+build.7", "1.2.3-rc.1+build")
        invalid = (
            "1",
            "1.2",
            "01.2.3",
            "1.02.3",
            "1.2.03",
            "1.2.3-01",
            "1.2.3-",
            "1.2.3+",
            f"{2**31}.0.0",
            "1.2.3 " ,
        )
        for version in valid:
            candidate = manifest_object()
            candidate["version"] = version
            self.assertEqual(xm.parse_manifest(encode(candidate)).version, version)
        for version in invalid:
            candidate = manifest_object()
            candidate["version"] = version
            with self.subTest(version=version), self.assertRaises(xm.ManifestError):
                xm.parse_manifest(encode(candidate))

    def test_target_is_horizon_only_and_versioned(self):
        for field, value in (
            ("architecture", "x86_64"),
            ("abi", "freebsd-elf"),
            ("abi_version", 0),
            ("abi_version", 2),
        ):
            candidate = manifest_object()
            candidate["target"][field] = value
            with self.subTest(field=field, value=value), self.assertRaises(xm.ManifestError):
                xm.parse_manifest(encode(candidate))

    def test_single_spdx_identifier_and_pinned_https_source_are_required(self):
        for license_text in ("", "MIT OR Apache-2.0", "GPL 2.0", "x" * 65):
            candidate = manifest_object()
            candidate["license"] = license_text
            with self.subTest(license=license_text), self.assertRaises(xm.ManifestError):
                xm.parse_manifest(encode(candidate))
        for url in (
            "http://example.invalid/source.tar",
            "https://user:secret@example.invalid/source.tar",
            "https://example.invalid/source.tar#fragment",
            "https://example.invalid/source.tar#",
            "https://exa%2fmple.invalid/source.tar",
            "https://example.invalid:/source.tar",
            "https://[::1]extra/source.tar",
            "https://example.invalid<evil>/source.tar",
            "https://example.invalid|evil/source.tar",
            "https://example.invalid/source\x7f.tar",
            "https://exa mple.invalid/source.tar",
            "https://例.invalid/source.tar",
            "https://example.invalid\\evil/source.tar",
            "https://exa%mple.invalid/source.tar",
            "https://example.invalid/%ZZ/source.tar",
            "https://example.invalid/source.tar?channel=%2G",
            "not-a-url",
        ):
            candidate = manifest_object()
            candidate["source"]["url"] = url
            with self.subTest(url=url), self.assertRaises(xm.ManifestError):
                xm.parse_manifest(encode(candidate))
        escaped_url = "https://example.invalid/source%20name.tar?channel=stable%2Fv1"
        candidate = manifest_object()
        candidate["source"]["url"] = escaped_url
        self.assertEqual(xm.parse_manifest(encode(candidate)).source.url, escaped_url)
        candidate = manifest_object()
        candidate["source"]["sha256"] = "A" * 64
        with self.assertRaises(xm.ManifestError):
            xm.parse_manifest(encode(candidate))


class FileInventoryTests(unittest.TestCase):
    def test_entrypoint_must_be_one_nonempty_inventory_nro(self):
        candidates = []
        missing = manifest_object()
        missing["entrypoint"] = "bin/missing.nro"
        candidates.append(missing)
        empty = manifest_object()
        empty["files"][0]["size"] = 0
        candidates.append(empty)
        wrong_suffix = manifest_object()
        wrong_suffix["entrypoint"] = "bin/example-tool.exe"
        wrong_suffix["files"][0]["path"] = wrong_suffix["entrypoint"]
        candidates.append(wrong_suffix)
        for value in candidates:
            with self.subTest(value=value), self.assertRaises(xm.ManifestError):
                xm.parse_manifest(encode(value))

        upper = manifest_object()
        upper["entrypoint"] = "bin/example-tool.NRO"
        upper["files"][0]["path"] = upper["entrypoint"]
        self.assertEqual(xm.parse_manifest(encode(upper)).entrypoint, upper["entrypoint"])

    def test_paths_reject_escape_reserved_and_nonportable_components(self):
        invalid = (
            "/absolute/tool.nro",
            "../tool.nro",
            "bin/../tool.nro",
            "bin//tool.nro",
            "bin\\tool.nro",
            "C:/tool.nro",
            "bin/tool?.nro",
            "bin/tool .nro ",
            "bin/trailing./tool.nro",
            "bin/CON.nro",
            "bin/CON .nro",
            "bin/CONIN$.nro",
            "bin/conout$.txt/tool.nro",
            "bin/COM¹.nro",
            "bin/com².txt/tool.nro",
            "bin/LPT³.nro",
            "bin/Lpt1.txt/tool.nro",
            "bin/control\x1f.nro",
            "bin/e\u0301.nro",
            "x" * (xm.MAX_COMPONENT_BYTES + 1) + "/tool.nro",
        )
        for path in invalid:
            candidate = manifest_object()
            candidate["entrypoint"] = path
            candidate["files"][0]["path"] = path
            with self.subTest(path=path), self.assertRaises(xm.ManifestError):
                xm.parse_manifest(encode(candidate))

    def test_ascii_case_and_file_directory_aliases_are_rejected(self):
        case_alias = manifest_object()
        case_alias["files"].append({
            "path": "BIN/EXAMPLE-TOOL.NRO",
            "size": 1,
            "sha256": "2" * 64,
        })
        prefix_alias = manifest_object()
        prefix_alias["files"].append({
            "path": "bin",
            "size": 1,
            "sha256": "2" * 64,
        })
        prefix_alias["files"].append({
            "path": "bin-interleaves-before-slash",
            "size": 1,
            "sha256": "3" * 64,
        })
        for value in (case_alias, prefix_alias):
            with self.assertRaises(xm.ManifestError):
                xm.parse_manifest(encode(value))

    def test_distinct_unicode_expansion_names_are_not_ascii_aliases(self):
        value = manifest_object()
        value["files"].extend([
            {"path": "share/Straße.dat", "size": 1, "sha256": "2" * 64},
            {"path": "share/STRASSE.dat", "size": 1, "sha256": "3" * 64},
        ])
        manifest = xm.parse_manifest(encode(value))
        self.assertIn("share/Straße.dat", {record.path for record in manifest.files})
        self.assertIn("share/STRASSE.dat", {record.path for record in manifest.files})

    def test_file_hash_size_count_and_total_are_bounded(self):
        candidates = []
        bad_hash = manifest_object()
        bad_hash["files"][0]["sha256"] = "g" * 64
        candidates.append(bad_hash)
        too_large = manifest_object()
        too_large["files"][0]["size"] = xm.MAX_FILE_BYTES + 1
        candidates.append(too_large)
        over_total = manifest_object()
        over_total["files"] = [
            {
                "path": "bin/example-tool.nro" if index == 0 else f"data/{index}.bin",
                "size": xm.MAX_FILE_BYTES,
                "sha256": f"{index + 1:x}" * 64,
            }
            for index in range(5)
        ]
        candidates.append(over_total)
        over_count = manifest_object()
        over_count["files"] = [over_count["files"][0]] + [
            {"path": f"data/{index:03}.bin", "size": 0, "sha256": "2" * 64}
            for index in range(xm.MAX_FILES)
        ]
        candidates.append(over_count)
        for value in candidates:
            with self.assertRaises(xm.ManifestError):
                xm.parse_manifest(encode(value))

    def test_maximum_file_inventory_is_deterministic(self):
        value = manifest_object()
        value["files"] = [value["files"][0]] + [
            {"path": f"data/{index:03}.bin", "size": index, "sha256": "2" * 64}
            for index in range(xm.MAX_FILES - 1)
        ]
        parsed = xm.parse_manifest(encode(value))
        self.assertEqual(len(parsed.files), xm.MAX_FILES)
        self.assertEqual(xm.manifest_digest(parsed), xm.manifest_digest(parsed))


class RelationshipAndCapabilityTests(unittest.TestCase):
    def test_capability_names_match_the_merged_safe_mode_header(self):
        repository = Path(__file__).resolve().parents[1]
        source = (repository / "exodus" / "security" / "capability_policy.h").read_text(
            encoding="utf-8"
        )
        capability_enum = source.split("} ex_capability;", 1)[0]
        identifiers = {
            name.lower()
            for name in re.findall(r"\bEX_CAP_([A-Z0-9_]+)\s*=", capability_enum)
            if name != "UNKNOWN"
        }
        self.assertEqual(
            identifiers,
            set(xm.POLICY_ALLOWED | xm.PROVIDER_REVIEW_REQUIRED | xm.POLICY_DENIED),
        )

    def test_requirements_are_sorted_and_exactly_pinned(self):
        value = manifest_object()
        value["dependencies"] = [
            {"name": "z-runtime", "version": "2.0.0"},
            {"name": "a-runtime", "version": "1.0.0"},
        ]
        manifest = xm.parse_manifest(encode(value))
        self.assertEqual([item.name for item in manifest.dependencies], ["a-runtime", "z-runtime"])

        mutations = []
        duplicate = manifest_object()
        duplicate["dependencies"] *= 2
        mutations.append(duplicate)
        self_dependency = manifest_object()
        self_dependency["dependencies"] = [{"name": self_dependency["name"], "version": "1.0.0"}]
        mutations.append(self_dependency)
        overlap = manifest_object()
        overlap["conflicts"] = copy.deepcopy(overlap["dependencies"])
        mutations.append(overlap)
        ranged = manifest_object()
        ranged["dependencies"][0]["version"] = ">=2.0.0"
        mutations.append(ranged)
        extra = manifest_object()
        extra["dependencies"][0]["optional"] = True
        mutations.append(extra)
        for value in mutations:
            with self.assertRaises(xm.ManifestError):
                xm.parse_manifest(encode(value))

    def test_safe_mode_capabilities_are_bucketed_without_granting_authority(self):
        value = manifest_object()
        value["capabilities"] = ["read_local_status"]
        allowed = xm.inspect_manifest(encode(value))
        self.assertEqual(allowed.capability_admission.disposition, "policy_allowed")
        self.assertEqual(allowed.capability_admission.allowed, ("read_local_status",))
        self.assertFalse(hasattr(allowed, "activate"))
        self.assertFalse(hasattr(allowed, "install"))

        value["capabilities"] = ["read_local_file", "read_local_status"]
        review = xm.inspect_manifest(encode(value)).capability_admission
        self.assertEqual(review.disposition, "provider_review_required")
        self.assertEqual(review.review_required, ("read_local_file",))

        value["capabilities"] = ["nintendo_eshop", "read_local_status"]
        denied = xm.inspect_manifest(encode(value)).capability_admission
        self.assertEqual(denied.disposition, "denied")
        self.assertEqual(denied.denied, ("nintendo_eshop",))

        value["capabilities"] = ["future_unreviewed_capability"]
        unknown = xm.inspect_manifest(encode(value)).capability_admission
        self.assertEqual(unknown.disposition, "denied")
        self.assertEqual(unknown.unknown, ("future_unreviewed_capability",))

    def test_capability_identifiers_are_unique_bounded_and_canonical(self):
        for capabilities in (
            ["read_local_status", "read_local_status"],
            ["Uppercase"],
            ["contains-dash"],
            [""],
            ["x" * 65],
            list(f"cap_{index}" for index in range(xm.MAX_CAPABILITIES + 1)),
        ):
            value = manifest_object()
            value["capabilities"] = capabilities
            with self.subTest(capabilities=capabilities[:3]), self.assertRaises(xm.ManifestError):
                xm.parse_manifest(encode(value))

    def test_manifest_helpers_require_exact_contract_objects(self):
        manifest = xm.parse_manifest(encode(manifest_object()))

        class Derived(xm.HorizonManifest):
            pass

        for value in ({}, object()):
            with self.subTest(value=value), self.assertRaises(xm.ManifestError):
                xm.manifest_digest(value)
        derived = Derived(
            manifest.schema,
            manifest.name,
            manifest.version,
            manifest.target,
            manifest.entrypoint,
            manifest.files,
            manifest.dependencies,
            manifest.conflicts,
            manifest.license,
            manifest.source,
            manifest.capabilities,
        )
        with self.assertRaises(xm.ManifestError):
            xm.classify_capabilities(derived)

    def test_profile_objects_revalidate_direct_construction(self):
        manifest = xm.parse_manifest(encode(manifest_object()))
        with self.assertRaises(xm.ManifestError):
            replace(manifest.target, abi="freebsd-elf")
        with self.assertRaises(xm.ManifestError):
            replace(manifest.files[0], size=True)
        with self.assertRaises(xm.ManifestError):
            replace(manifest.source, url="file:///tmp/source")
        with self.assertRaises(xm.ManifestError):
            replace(manifest, files=tuple(reversed(manifest.files)))


if __name__ == "__main__":
    unittest.main()

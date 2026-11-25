# EXODUS xpkg Horizon manifest R01

R01 is a pure-stdlib, offline metadata boundary for EXODUS-owned packages that
target the Horizon NRO lifecycle. It is the first bounded contract under #87;
it does not duplicate FreeBSD `pkg` or Ports.

The implementation is `exodus.xpkg_manifest`. It accepts exact `bytes`, parses
one versioned JSON object and returns immutable normalized records. It does not
open a path, inspect or extract an archive, contact a repository, verify a
signature, install content, write to SD/NAND, or execute the entrypoint.

## Wire schema

The schema identifier is `exodus.xpkg.horizon-manifest/1`. R01 requires all and
only these top-level fields:

```json
{
  "schema": "exodus.xpkg.horizon-manifest/1",
  "name": "example-tool",
  "version": "1.2.3",
  "target": {
    "architecture": "aarch64",
    "abi": "horizon-nro",
    "abi_version": 1
  },
  "entrypoint": "bin/example-tool.nro",
  "files": [
    {
      "path": "bin/example-tool.nro",
      "size": 4096,
      "sha256": "0000000000000000000000000000000000000000000000000000000000000000"
    }
  ],
  "dependencies": [
    {"name": "runtime-assets", "version": "2.0.0"}
  ],
  "conflicts": [],
  "license": "MIT",
  "source": {
    "url": "https://example.invalid/example-tool-1.2.3.tar.gz",
    "sha256": "1111111111111111111111111111111111111111111111111111111111111111"
  },
  "capabilities": ["read_local_status"]
}
```

Package and requirement names are lowercase ASCII identifiers. Versions are
canonical bounded SemVer values; dependencies are exact pins in R01 rather
than ranges. `license` is one bounded SPDX-shaped identifier, not an arbitrary
expression; R01 checks its wire syntax but does not consult the SPDX registry.
The source URL must be credential-free HTTPS without a fragment, backslash or
malformed percent escape, and is paired with a lowercase SHA-256 value. The authority must have an ASCII hostname or a bracketed IP literal, optionally followed by a numeric port; escaped authorities and empty fragments are rejected. Valid
percent escapes are preserved rather than normalized. The source digest
records provenance; it does not authenticate the package manifest. A later
network stage remains responsible for fetching policy and redirect handling.

## File inventory

The manifest contains only regular-file metadata. It has no symlink, device,
install-script, owner, setuid or arbitrary mode representation.

- The input is at most 64 KiB and the JSON depth is at most eight.
- There are at most 256 files, 16 GiB per record and 64 GiB total.
- Paths are relative NFC UTF-8 text with bounded components.
- Empty, dot, dot-dot, absolute, drive and backslash components are rejected.
  C0 (`U+0000–U+001F`), DEL (`U+007F`) and C1 (`U+0080–U+009F`) control
  characters are rejected anywhere in a path. DOS/Windows device stems are
  also rejected for portable staging,
  including `CONIN$`, `CONOUT$` and the superscript `COM¹`/`LPT¹` variants.
- ASCII case aliases and file/directory prefix aliases are rejected before a
  later archive stage can write them to FAT-style storage.
- Non-ASCII characters are preserved. R01 deliberately does not apply broad
  Unicode case folding, which can collapse distinct names such as expansion
  pairs. A later extractor must enforce the actual destination filesystem's
  comparison rules.
- The entrypoint must identify exactly one nonempty inventory record whose
  suffix is `.nro` under ASCII case comparison.

The parser sorts files, requirements and capabilities into a canonical order.
`canonical_bytes()` and `manifest_digest()` therefore produce the same
consistency digest for semantically identical input ordering and whitespace.
The digest is not a signature, MAC, trust decision or content verification.

## Nintendo Safe Mode classification

`inspect_manifest()` applies the merged R01 capability names from
`exodus/security/capability_policy.h` without granting execution authority.
Its `capability_admission.disposition` is one of:

- `policy_allowed`: every declaration is currently read-only policy metadata
  (`read_local_status` or `read_local_catalog`);
- `provider_review_required`: at least one known declaration still requires a
  separately implemented and reviewed provider boundary, such as local file,
  LAN, SSH or local launch access;
- `denied`: a restricted or unknown declaration is present.

Restricted declarations include Nintendo eShop, Nintendo Account, Nintendo
Switch Online/official services, license mutation, system-content install,
unreviewed native plugins and raw privileged dispatch. Unknown names are
retained in the inspection report but fail closed as denied. There is no
caller-supplied approval flag and no `install()` or `activate()` method.
The focused suite also compares these known names with the merged C Safe Mode
enum, so a policy enum change must update this admission contract explicitly.

Parsing success alone never authorizes a package. Even a `policy_allowed`
report still needs, in later #87 work, a trusted signed index, manifest
signature/key policy, archive content verification, safe extraction,
transaction journal, storage admission, explicit operator action and a
separately reviewed runtime provider.

## API and tests

```python
from exodus.xpkg_manifest import inspect_manifest

inspection = inspect_manifest(manifest_bytes)
print(inspection.canonical_sha256)
print(inspection.capability_admission.disposition)
```

Run the focused suite with:

```text
python -m unittest tests.test_xpkg_manifest -v
```

The tests cover duplicate and unknown fields, nonstandard JSON numbers,
primitive types, SemVer, target isolation, path aliases/traversal, inventory
budgets, relationship conflicts, deterministic normalization, detached output
and all three capability dispositions. They are host evidence only; no Switch
hardware, archive, repository, signature, installation or recovery claim is
made by R01.

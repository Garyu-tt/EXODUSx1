# EX-011: Release integrity and recovery

Status: first read-only inventory implementation; not a firmware installer.

## Usage

Run from repository root with Python 3.8+:

    python -m exodus.release_inventory 4IFIR.zip > upstream.json
    python -m exodus.release_inventory EXODUSx1.zip > candidate.json
    python -m exodus.release_inventory EXODUSx1.zip --verify candidate.json
    python -m unittest discover -s tests -p test_release_inventory.py -v

Inventory records sorted file paths, byte sizes and SHA-256. It rejects common unsafe paths, duplicate or case-colliding full paths, file/directory conflicts, symlinks, encrypted entries and oversized payloads. It never extracts anything. Verification detects changes against a trusted inventory, not authenticity; an unsigned manifest from the same untrusted source is insufficient. Empty directories and ZIP container metadata are not covered. This is not a complete FAT32 path validator or hostile-archive sandbox.

## Release work packages
- [ ] Pin actual 4IFIR archive URL/version and digest; identify Latitude, MTP, Atmosphere and actual Windows runtime binaries by provenance. Do not infer identity merely from file names.
- [ ] Record licenses, source revisions, build flags and HOS/console compatibility. Do not redistribute proprietary components without permission.
- [ ] Build EXODUS-owned modules with pinned devkitPro/libnx. Produce reproducible staging tree and deterministic ZIP; compare independent builds.
- [ ] Add signed release manifest, trusted verification key, CI and compatibility matrix; reject incompatible builds.
- [ ] Add backup and safe rollback procedure with emuMMC hardware tests, preserve user files and NAND. No unattended NAND writes, no automatic installation or clock changes.
- [ ] Verify boot, HOME, USB, rollback, native and Windows app paths on actual Switch. Track partial features separately; do not label an untested package a release.

EX-009 App Manager and EX-012 Developer Mode should consume verified release metadata. EX-010 Input Engine remains separate. EX-006/007/008 have their own acceptance gates.

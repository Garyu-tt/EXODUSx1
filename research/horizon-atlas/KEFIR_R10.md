# Kefir Hub R10 donor audit

Build manifest: 413 unique source paths (including optional demo sources). Ten source files inspected and linked to 23 existing requirements. Source inspection is not a dependency closure or tested port. The CMake target also links SMB2, NFS, curl, physfs, archive, compression, NanoVG and other libraries; individual licenses and versions must be checked. GPLv3 text present in repository LICENSE. No donor code copied.

## First integration slices (proposals)
1. Path and URL utilities: independent host tests, FAT case handling, bounded path normalization, reject traversal and mount escapes. Verify license compatibility.
2. VFS: native/virtual/stdio interfaces, explicit owned mounts and descriptor lifetimes; test partial I/O, error translation, rename and close. No protected storage.
3. Transfer pipeline: bounded buffers, cancellation, worker cleanup; isolate UI dependency and do not inherit installer paths.
4. NRO discovery and local catalog: reuse evidence for scanning and invalidation; connect to existing EXODUS HOME catalog rather than a second catalog. No title forwarder installation.
5. Read-only network status: use verified NIFM atoms and separate any radio/network mutations.
6. Image viewport and application translations: portable geometry and UI-only localization.

## Exclusion gate
Do not import Kefir Hub wholesale: its build includes protected content management, account linking, key extraction, process modification and hardware/system changes. Mixed web router and file routes require independent authentication, scoped paths, allowlisted operations and denial of protected install routes. No donor code is admitted at R10.

Observed in upstream documentation: a thread-start failure teardown gap in ProgressBox/download lifecycle. Treat as an audit item, not a verified bug reproduction. Host tests and hardware checklist exist; not run here. Standalone user fork was not found at last owner-list check, so upstream sources are used.

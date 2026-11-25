# EVCS transfer (R15)

Host-side optional transfer into an existing, owner-controlled Games
directory. This is a file copy, not installation or authorization.

```sh
python -m exodus.evcs_transfer /source/Game.nsp /sd/Games --fat32
python -m exodus.evcs_transfer /source/Game.nsp /sd/Games --fat32 --execute
python -m exodus.evcs_transfer /source/Game.nsp /sd/Games --fat32 --execute --resume
```

Without --execute only the existing read-only preflight runs.
The transaction hashes the source, records a journal under
Games/.evcs-stage-<token>, copies to a non-game-named payload,
flushes and verifies it, then publishes without replacing an existing
file or directory. Linux uses renameat2 with RENAME_NOREPLACE;
Windows uses os.rename. Unsupported filesystems/platforms fail closed.
Source files are never removed.

Explicit resume requires the exact same source/destination, source
fingerprints and hashes. Verified staged parts are reused, incomplete
journal-bound regular files are recopied. A matching already-published
destination can be verified after interruption. The journal is retained.
An incomplete or mismatched journal stops safely; inspect it manually.
Indirect staging directories, reparse points, and multiply linked staging
files are rejected. Preflight refusal returns exit 1 without copying.
No automatic cleanup of game content. Files with EVCS sidecars are
rejected until paired transfer is implemented.

Single writer, stable owner-controlled paths, and supported filesystem
semantics are prerequisites. This is not a sandbox against concurrent
filesystem attacks. File fsync and Linux directory fsync are used, but
physical FAT32 power-loss behavior and Windows publication have not yet
been tested. Recovery across remounts that change device/inode identity
may stop safely. No direct launch, rights or registration change.

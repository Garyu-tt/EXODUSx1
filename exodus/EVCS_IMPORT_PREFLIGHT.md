# EVCS import preflight (R14)

Run from the repository root on a host with the SD card mounted:

```sh
python -m exodus.evcs_import_plan /path/to/Game.nsp /path/to/SD/Games --fat32
```

The Games directory must already exist. The command only reads filesystem
metadata and returns JSON. It supports nonempty NRO/NSP/XCI regular files
and NSP/XCI directories containing contiguous numbered parts 00..99.
Existing files in Games require no additional payload copy for discovery.
For external files it reports a conservative payload-copy estimate, free
space, case-insensitive name collisions, logical size, allocated source
blocks when the OS exposes them, and same-device status.

The estimate excludes filesystem overhead, other concurrent writes and
optional sidecars. Same-device status is not proof that rename is supported,
and no actual transfer is performed. A structurally plausible split is
not proof of complete or authentic content. The tool rejects parent traversal, drive-relative paths, symbolic links and
Windows reparse points, including parent directories and split members.
It assumes stable, owner-controlled paths; it is not a sandbox against
concurrent changes.

## Execution gate

No install, move, deletion, registration or launch is performed. No game
rights or signature policy changes. Any subsequent transfer needs explicit
approval, fresh validation, a supported destination, verified completion
and interruption recovery. Never automatically remove the source.

NCM RegisterPath is documented only on Horizon 13.0.0+:
https://switchbrew.github.io/libnx/ncm_8h_source.html
The Host storage enum is not evidence of an authorized retail path.
On the older target firmware, native one-copy registration remains
unverified; retain the existing supported launch routes and do not
advertise archive-only launch.

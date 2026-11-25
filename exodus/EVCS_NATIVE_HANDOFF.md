# EVCS native frontend handoff (R16)

Export a read-only JSON descriptor list from the existing catalog:

```sh
python -m exodus.native_descriptors catalog.json
```

Each descriptor has a stable provider-specific key, argument vector,
confirmation requirement and unverified launch status. Switch IDs are
canonical lowercase hexadecimal; homebrew accepts only relative paths
under sdmc:/switch/, matching the currently admitted native adapter.
Absolute Games paths, archives and Steam/Windows entries do not become
native targets in this integration. The catalog must use all eight v1
entry fields; duplicate exact provider identities are rejected.

A frontend capable of starting the native EXODUS shell can pass the
argument vector (no shell interpolation):

```
--native-target switch 010000000000abcd
--native-target homebrew tools/demo.nro
```

The Switch shell loads its default font and asks for explicit
confirmation, then uses existing Horizon inventory/applet checks or
the validated homebrew loader route. It returns 0 for an accepted
request (not evidence that execution occurred), 1 for unavailable
handoff, 2 for invalid arguments, 3 for canceled confirmation.
The host executable refuses native handoff.

This is an interface contract, not a port of Steam, a Steam shortcut
installer, a remote launch server or a verified return-to-library
mechanism. The future SteamOS bridge must invoke the native shell
through a supported local backend and preserve launch/exit semantics.
No game content, Steam configuration, Nintendo rights or boot changes.
Actual loader argument transport, full SDL/native compilation and
physical HOS compatibility remain acceptance gates.

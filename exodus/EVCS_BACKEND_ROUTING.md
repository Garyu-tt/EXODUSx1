# Backend-first launch architecture (R17)

Presentation (Games / Applications), software source, executable ABI and
execution backend are independent dimensions. Homebrew is an application
category, not a child of Nintendo. The existing catalog provider strings
and favorites remain compatible during migration.

## Steam integration contract

Use the same custom compatibility-tool discovery mechanism as Proton:
`~/.steam/root/compatibilitytools.d/`. Each real adapter distribution
will contain `compatibilitytool.vdf`, `toolmanifest.vdf` and an executable
adapter. References: ValveSoftware/Proton README,
compatibilitytool.vdf.template and toolmanifest_x86_64.vdf.

Proposed tool IDs: exodus-horizon and exodus-bsd (plus CorkDeck only if
its adapter is validated). Do not impersonate Proton or claim Windows
compatibility to force a tool into the selector. Steam's accepted OS
identifiers, non-Steam shortcut behavior, and the EXODUS Steam port's
actual discovery semantics must be tested before publishing manifests.

Horizon receives typed Title ID or NRO targets and delegates to the
existing guarded launch route. BSD receives a separately validated ELF
target; its real execution adapter is not connected yet. Proton remains
a separate Windows compatibility tool. Steam catalog ownership must not
determine backend selection.

## Acceptance gates

- Verify custom-tool discovery and selection in the actual Steam client.
- Confirm argument/verb handling, trusted local transport, process
  lifetime, exit propagation and return to the library.
- Keep missing backends unavailable, with no fallback into another ABI.
- Do not install placeholder tools, modify Steam user configuration,
  perform native archive registration or claim launch support before
  the corresponding adapter is verified.

R17 implements the typed backend/target dispatch and regression cases,
not a working Steam compatibility-tool package. Existing UI categories
are preserved until their separate Applications presentation is wired.

## R18: local bridge protocol (host-only integration)

The `steam_compat` module validates bounded `.exodus-target` descriptors and previews non-installable VDF manifests. The `steam_bridge` client and server use bounded JSON frames over an owner-only Unix socket, a random request ID, explicit confirmation requirement and accepted/running/exited lifecycle transitions. A missing adapter rejects requests. No automatic launch retry, shell execution, external listener, or exit inference from disconnection is provided. Both client verbs wait for a terminal event. Tests exercise simulated adapters, not actual game execution.

This integration intentionally excludes the separate Horizon observer, managed applet session and Applications UI changes from #160. No native adapter is installed and no actual Steam discovery, FreeBSD execution or Switch hardware acceptance is claimed.

## Native lifecycle evidence (separate integration)

The `--native-observe` diagnostic samples the application PID and program ID with a PID recheck; unavailable permissions or inconsistent samples report `unknown`. A managed SystemApplet session helper requires verified target and user confirmation, and does not infer a POSIX exit code from Horizon's raw exit reason. It remains unconnected to the bridge and is not enabled in the ordinary shell UI. Host mock tests cover PM failures, cleanup, permission/context gates and lifecycle transitions. Real permissions, native session and hardware coexistence remain unverified.

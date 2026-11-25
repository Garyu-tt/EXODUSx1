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

## R18 local bridge transport

`python -m exodus.steam_bridge --socket /private/runtime/bridge.sock exodus-horizon waitforexitandrun game.exodus-target`

Client and one-connection server library use bounded newline-delimited
JSON over an owner-only Unix socket, with a random request ID, typed
Horizon target and mandatory confirmation. Both run verbs wait for
observed accepted -> running -> exited events; cancellation/rejection/
failure are distinct terminal states. Disconnect, deadline or invalid
event never implies exit. No retry or termination of an unknown target.

The server validates again before invoking its injected trusted adapter.
Without a real adapter it rejects. Listener creation, endpoint ownership,
same-user trust, lifecycle observation, concurrency and the actual Horizon
handoff are platform integration responsibilities. No network endpoint or
arbitrary executable command is exposed. The client defaults to a one-hour
deadline, configurable up to 24 hours; expiration does not cancel a game.

Local socket tests use simulated adapters. They do not prove the native
Horizon bridge, process lifetime or Steam discovery on target hardware.

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

## R18 listener and native observation

The host can now create an owner-only Unix socket using
`steam_bridge_listener.listener(path)` and dispatch one connection at a
time with `serve_once(server, adapter)`. No default adapter is installed.
Existing endpoints are not replaced or removed. The caller must supply
a private canonical directory and a trusted local adapter.

The native shell exposes `--native-observe` before UI initialization.
It reads the application process ID from pm:dmnt and resolves the program
ID through pm:info. It checks the PID again to reject an inconsistent
sample and closes initialized services on all paths. Host builds and
service failures report unknown, not an exit. A newly observed matching
PID is evidence of process presence only; a matching preexisting process
does not establish a new launch. No debug attach, privilege changes,
termination, or automatic fallback.

A persistent trusted native observer and supported application-accessor
lifecycle events are still required before the bridge can report a real
game's exit. libnx documents appletCreateApplication as SystemApplet-only:
https://switchbrew.github.io/libnx/applet_8h.html
Do not assume that an ordinary NRO can obtain that role. Homebrew NRO
handoff also requires separate observation because the host process
can persist across loader transitions. Local PM test shims do not prove
hardware permissions or SDK compatibility.

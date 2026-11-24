# EXODUS Remote R01

Tracks #89; package operations track #87. This is a transport-independent C11
reference dispatcher, **not** an SSH server, network listener, authenticator,
or hardware provider. No console services are enabled by this change.

Requests are exact ASCII operation IDs, 1–32 bytes, with no arguments.
Responses are bounded to 512 bytes including terminator. Discovery lists only
operations that intersect session grants, local provider review and availability.
Unknown commands are rejected. No generic shell, file write, Nintendo service,
privileged IPC or power operation exists in this release.

The transport must authenticate a paired client, apply local opt-in and policy,
enforce session revocation/expiry, serialize access to state, and supply a
monotonic clock. Session and provider structs are trusted local state, not wire
formats. Providers must respect output capacity and own their validation.
Do not expose this dispatcher directly to the network or treat its boolean
fields as credentials. Do not advertise mock providers as hardware evidence.

## Host verification

    cc -std=c11 -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined test_remote_gate.c -o /tmp/exr-test
    /tmp/exr-test

Local test passed with mock providers only. No devkitA64 or physical Switch
verification was performed. Run from this directory.

## Next integration

- Review the existing capability policy (#52), native terminal (#43/#82),
  SSH client (#37/#83) and capability atlas (#62); these are separate PRs.
- Implement read-only Horizon providers with real result codes; default to
  unavailable on probe failure. Verify on target hardware.
- Add authenticated encrypted transport (review existing SSH server library
  feasibility; libssh2 is a client library), local pairing, key revocation,
  deadlines and audit trail. Use LAN/VPN only with owner opt-in.
- Integrate shared policy before enabling any network listener; add transport
  framing and request-size/rate limits, and test disconnect/revocation.
- Add typed package operations once #87 is ready; no unrestricted remote shell
  in the initial deployment. Agent/Adamantium adapter remains host-side.


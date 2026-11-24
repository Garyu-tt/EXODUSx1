# Nintendo Safe Mode (P0 release requirement)

EXODUS must hide and disable capabilities that could expose a console or account to Nintendo enforcement risk. This is risk avoidance, not stealth or evasion. No guarantee against bans is implied.

## Mandatory behavior

- Hide and disable Nintendo eShop, Nintendo Account, Nintendo Switch Online and EXODUS integration with official Nintendo network services.
- Deny unclassified or high-risk capabilities by default. Classify each capability with risk, rationale and review status.
- Remove risky entry points from launcher, settings, menus, search, shortcuts, CLI discovery and plugin catalog. Independently deny execution in backend/API, deep links, terminal integrations and plugin interfaces.
- No manipulation of certificates, identities, telemetry, entitlements, tickets, licenses or enforcement mechanisms. No spoofing or evasion.
- Keep ordinary internet, LAN, SSH, BSD terminal and permitted local homebrew available under least privilege; none may implicitly bypass the capability policy.
- Separate EXODUS settings from stock Horizon settings. Do not alter stock firmware or account state. No claim of protection for official services outside EXODUS.
- No ordinary user toggle that unlocks high-risk functions. Release builds fail closed.

## Acceptance gates

1. Inventory all capabilities and classify risk before enabling them.
2. Test that restricted functions are absent from UI/discovery and denied through backend, CLI and plugins, including direct invocation.
3. Test that EXODUS makes no prohibited official-service requests while permitted LAN and internet remain available.
4. Regression-test startup, SSH and permitted local homebrew.
5. Require policy review and passing tests before release. This document is a specification, not evidence of implemented enforcement.

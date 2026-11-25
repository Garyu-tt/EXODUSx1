# Upstream baseline for EXODUSx1 R0.1

The pinned release metadata is in 4ifir.lock.json. Do not silently follow latest.
Obtain the archive from the pinned asset URL and run from the repository root:

    python -m exodus.verify_upstream 4IFIR.zip

Only after success, use the read-only release inventory from PR #6 when available. Record the verified full file tree and exact versions/source provenance of the shell, MTP service, Windows runtime, bootloader and performance modules. The April 26 checker index (last check May 18) is historical, not evidence for the November 23 archive. The archive itself was not downloaded in this development environment due to DNS failure. GitHub release digest is metadata, not independent binary validation or proof of a publisher signature.

## Integration discoveries
- Upstream [Latitude documentation](https://github.com/rashevskyv/4IFIR/blob/main/docs/LATITUDE_UI.md) describes a qLaunch replacement, R+eShop invoking Ownfoil, existing file manager/homebrew store, built-in MTP and native updates. These are documented behavior, not source-level identification.
- [Composition documentation](https://github.com/rashevskyv/4IFIR/blob/main/docs/COMPOSITION_4IFIR.md) names SaltyNX, FPSLocker, sys-clk, Ultrahand and status monitoring. Prefer adapters over duplicate services. Confirm exact installed variants.
- No actual Latitude or Windows runtime binary identity has been established. Do not patch an assumed component or publish an installable firmware based solely on the older index.

## Device integration gates
- EX-004 MTP: verify enumeration and transfer of a user-owned test file with hash checks, USB reconnect, sleep/wake, cable variations, no conflicting USB service. Log host and device outcomes, do not restart while writing.
- EX-001/002/003 Windows: identify runtime and exact version, capture logs for GTA logo hang, broken installer rendering and unsupported Steam OS message, reproduce independently and isolate changes.
- EX-006/007: locate actual HOME shell source or legal replacement, confirm eShop and long HOME routes on real hardware before claiming integration.
- Release: preserve original files and emuMMC rollback, only stage legally redistributable components, no unattended NAND writes.

No console or user equipment was accessed in this step.

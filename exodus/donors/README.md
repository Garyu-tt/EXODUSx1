# EXODUS donor integration factory

EXODUS is a native Horizon operating environment, not a SteamOS distribution or a wholesale Linux port. Forks are research inputs, not dependencies automatically admitted to builds. See [registry.json](registry.json) for the provisional inventory.

Thanks to Cooler3D and the 4IFIR contributors for the original work and inspiration. Preserve upstream notices and verify each component's license and source provenance before copying or distributing it. The existing 4IFIR README already describes SteamGridDB artwork integration; audit and reuse its legitimately available implementation before replacing it.

## Integration contract
1. Identify exact upstream and fork commit, license, source files, public interface and dependency closure. Record evidence and immutable SHA in the registry.
2. Export a directed graph of functional atoms: capability ID, input/output contract, dependencies, resource/privilege requirements, implementation alternatives and test oracle. Classify native Horizon, portable, replaceable, or unsupported. Avoid duplicating the Kernel Bridge graph (#30); map donor IDs to existing graph nodes after those branches land.
3. Assign one owner to each shared capability. Reference Kernel Bridge provider ABI (#32), lifecycle (#33), service supervisor (#34), shell (#35), and scene/focus/input (#36). Do not create competing compositor or service APIs in separate donor repos.
4. Implement smallest portable slice, host fixtures and negative tests first, then native libnx/deko3d provider with explicit unsupported status for absent functionality. POSIX host success is not native success.
5. Cross-build on ARM64, measure RAM, frame time and startup; record physical console firmware, applet/application mode and device evidence. No firmware modifications or unsupported privileges as a prerequisite for this work.
6. Land independently reviewable PRs with rollback instructions and reproducible provenance. Keep upstream tracking branches unchanged; use named feature branches.

## Work queue
| Lane | First deliverable | Dependencies |
|---|---|---|
| P0 SDK | Map donor capability IDs to existing Kernel Bridge graph; versioned contracts for storage, clock, process, input, graphics, IPC and network | #30–36 |
| P0 Shell | Single controller-first library, NRO/title/runtime launch broker and recovery path | #18, #24, SDK |
| P0 Compositor | Evaluate Gamescope, KWin and Wayfire policies against #36; native surface and input probe | SDK, #36 |
| P1 Desktop | Minimal desktop/docked session; investigate Qt and Godot platform backends separately, no promise of full Plasma port | compositor |
| P1 Runtime | Extend Wine atomizer with Proton and Autorun; ARM64/guest ISA, ABI, loader and GPU capability tests | #8, #17, #19, SDK |
| P1 Input | Controller profiles and remapping without Linux evdev assumptions | SDK |
| P1 Telemetry | Shared read-only metrics provider; overlay with bounded cost | SDK, compositor |
| P1 Package/artwork | Native SD discovery, journaled package lifecycle, artwork cache and existing SteamGridDB integration audit | #24, shell |
| P2 Plugins | Manifest, explicit capabilities, lifecycle and isolation; no arbitrary system-service access | supervisor, SDK |
| Research | Determine actual scope of Switchdeck, CorkDeck, carafe and x1LiberNT; no blind code import | provenance |

## Admission
A candidate is not a port. It is admitted only with a pinned commit, verified licensing, mapped dependencies, host contracts, cross-build, physical-device evidence and rollback. Unsupported Linux-only features remain disabled. Preserve stock recovery, user data and existing NRO compatibility. Never present desktop screenshots or host-only simulations as device success.

Registry status is intentionally audit_required for every donor. Discovery branch names are observed hints, not pinned build inputs. Scope and first slices are proposals, not verified upstream APIs.

# Tumor R17: dual-kernel execution contract

Canonical owner decision: #103. Implementation queue: #105. This document
defines the target, not a claim of physical coexistence. Existing QEMU evidence
remains reference-only. No boot payload, firmware changes or raw register writes
are authorized by this document.

## Bootstrap gate

User-visible order: Horizon starts, then FreeBSD starts and becomes the control
plane. The required privileged mediation may have to be established by an
authorized boot environment **before** Horizon enters its kernel. Launching a
second privileged kernel from an ordinary NRO is not a supported assumption.
The candidate topology is two isolated kernel domains under a privileged
mediator, with mediated devices and bounded shared channels. The actual
privilege level, board-specific boot support and supported recovery path remain
unknown. Do not assume EL2 is available, and do not bypass security boundaries.
If the authorized platform cannot support coexistence, report BLOCKED rather
than calling dual boot or a BSD compatibility shim dual-kernel.

## Ownership contract

| Resource | Initial owner / required behavior |
|---|---|
| CPU | Mediator schedules both domains; no uncontrolled core ownership. Measure latency. |
| Physical RAM | Disjoint reservations and explicitly mapped shared buffers. Budget against actual target-available RAM. |
| Interrupts, timers and DMA | Mediated or assigned exclusively; prove isolation before enabling real devices. |
| GPU, display, HID | Initially keep Horizon's native game path. BSD rendering uses a validated mediated path or explicit exclusive handoff. No concurrent direct device ownership. |
| Wi-Fi | BSD owns logical network services and SSH endpoint; actual hardware driver owner is TBD and may require mediation. |
| SD / persistent storage | One writer per filesystem; separate volumes or validated file service. Never mount a writable filesystem in both kernels concurrently. |
| Nintendo title lifecycle | Horizon retains native execution, applet services and permissions; FreeBSD requests authorized actions through the bridge. |
| UI / configuration | FreeBSD service plane owns desired settings and session state; target UI is Steam Gaming Mode and KDE. |

## Runtime and admission gates

1. **Physical context**: identify exact console and authorized boot context;
   record evidence source and distinguish USB mass storage, EL0 diagnostic and
   privileged platform evidence. No raw writes. EL0 cannot independently
   establish EL2 ownership.
2. **Mediator**: demonstrate supported privileged startup, protected memory,
   disjoint CPU/RAM domains, IRQ/timer routing and DMA isolation using controlled
   tests in reference environments first. Hardware result required separately.
3. **Concurrent boot**: Horizon stays alive while a real ARM64 FreeBSD kernel
   starts and reports independent uptime. Preserve both identities and
   timestamps; repeated heartbeats must overlap without reboot or simulated
   kernel output. Establish bounded IPC and verify malformed message rejection,
   timeout, disconnect and peer restart. No raw memory or generic privileged
   shell capability.
4. **Resident profile**: start FreeBSD rc.d, OpenSSH and minimal management
   services. Keep them running during an authorized native title session.
   Observe SSH command/heartbeat before, during and after launch; record RAM,
   CPU, frame-time impact and service PIDs. Nonessential graphical processes
   may be suspended or stopped, but the BSD kernel and admin services stay up.
   Deep sleep and power-off are outside the always-on promise.
5. **UI/provider integration**: same catalog and settings registry for Steam
   Gaming Mode and KDE; prove display/HID handoff, title return, saves and
   recovery. Steam client and game ABI compatibility are separate admission
   gates. A closed binary cannot be presumed portable.
6. **Release**: physical evidence for every required node, measured resource
   budgets and explicit recoverable installation scope. No firmware changes
   or release promotion from source audit or QEMU results alone.

## Work allocation

Prioritize physical context, privileged mediation and CPU/RAM/IRQ/DMA
isolation. Continue independent FreeBSD reference, graphics and upstream
manager builds without claiming Switch acceptance. Retain #89 typed Horizon
provider work but stop duplicate Horizon SSH/TLS production services: external
SSH/SFTP lives on BSD. Reuse existing diagnostics only as evidence and where
the selected inter-kernel transport needs them.

The machine-readable graph.json is the dependency DAG; this contract explains
its hardware gates. #55 applies to all interfaces. Feature freeze: these
nodes elaborate existing #103/#105 requirements, not new product features.

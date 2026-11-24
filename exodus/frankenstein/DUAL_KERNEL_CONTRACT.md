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

## R19 candidate assessment and heartbeat contract

- [Jailhouse upstream](https://github.com/siemens/jailhouse) and [Jetson TX1/TX2 port](https://github.com/evidence/linux-jailhouse-jetson) establish a related Tegra partitioning precedent, not Switch support. Jailhouse requires a Linux control OS; use as an architectural donor only, not a third production OS.
- [FreeBSD 15.1 vmm(4)](https://man.freebsd.org/cgi/man.cgi?format=html&query=vmm&sektion=4) documents EL2 at boot and GICv3 on arm64. [Upstream Tegra210 device tree](https://code.googlesource.com/linux/torvalds/linux/%2B/6f771ce6c3bb2c4dc9d348b578871c7adb8b285b/arch/arm64/boot/dts/nvidia/tegra210.dtsi) describes GIC-400 (GICv2). Do not assume stock bhyve can host this physical platform. Actual Switch revision and virtual interrupt support remain to be established.
- [seL4 TX1 documentation](https://docs.sel4.systems/Hardware/jetsontx1.html) explicitly excludes SMP, SMMU and Hyp-mode in that kernel port; not a turnkey mediator for this target.
- `ipc_contract.py`: 24-byte fixed-size, big-endian reference heartbeat with version, role, generation and strict sequence; reject malformed/replayed/stale messages and close on local 3s timeout or clock rollback. Locally established generation and authenticated endpoint identity are assumptions of the reference model, not authentication provided by the frame itself. Two fresh heartbeats are necessary but not sufficient to prove independent kernel execution. No actual shared-memory driver, kernel, hardware mapping or privileged transport is implemented.
- Local Python tests: 7/7 groups PASS, including nine malformed-frame variants; exact Git blob 4a89d74f6ad97d0764e213b67498ab02f77e3791. CI matrix pending. Timeout is a test parameter, not a measured hardware budget.

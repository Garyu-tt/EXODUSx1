# Hardware Atlas R02 — Horizon driver overlay

Adds 118 source-evidence nodes and 212 edges to the R01 hardware and R07 Horizon graphs. Includes 87 legacy PCV module enumerators and 87 newer module IDs, 75 conceptual hardware-to-clock mappings, 8 driver paths, 8 NV IPC wrappers, 4 NV service endpoints and 11 candidate API capabilities. All endpoint references checked against the union of both prior graphs and this overlay. Source files pinned to libnx b156b4774fb2df7b93402169509266da7547fc4c; historical NV documentation identified by blob.

## Correction to R01

Historical NV channel documentation explicitly lists /dev/nvhost-msenc (Video Encoder), and the current pinned PCV header enumerates NVENC. This is a stronger source-level candidate than silicon-only evidence, not proof that a normal app can open or submit to the channel, or that a specific codec or resolution works. R01 evidence is preserved, not silently rewritten.

## Candidate research paths

| Block | Route | First acceptance target |
| --- | --- | --- |
| Maxwell | nvdrv, nvhost-gpu, nvhost-ctrl-gpu | Read-only characteristics, then owned-buffer rendering |
| VIC | nvhost-vic | Owned synthetic image scaling/color conversion |
| NVDEC | nvhost-nvdec | Decode a self-owned test clip; verify codec and frame output |
| MSENC/NVENC | nvhost-msenc | Verify authorized access before owned-frame encode |
| NVJPG | nvhost-nvjpg | Decode a small owned JPEG; compare CPU path |
| Display | VI and historical nvhost-display | Current resolution/vsync; no foreign layer control |
| PCV/clkrst | Clock module enumeration | Read-only telemetry where permitted; no clock or voltage writes |
| TS/TC/PSM | Temperature and battery services | Read-only temperature/battery observations |

NV service initialization selects nvdrv for application, nvdrv:a for applet, nvdrv:s for system and nvdrv:t for factory contexts. These are separate access boundaries, not fallback routes for circumventing permission denial. No factory endpoint use, permission changes or raw-register access. nvOpen, nvIoctl, nvIoctl2/3, nvClose and nvQueryEvent are wrapper-level IPC evidence; per-device command semantics and rights are distinct. All nodes remain non-executable, access unknown, device tests not run. No overclocking, raw I/O, foreign buffers, protected content or key handling.

Next: gather source examples for VIC/NVJPG/NVDEC/MSENC, trace buffer ownership and synchronization, build per-context admission matrix and owned synthetic workload harness. Actual performance/availability must be measured per console revision. Overlay is additive; no merged graph or rendered DOT produced in R02.

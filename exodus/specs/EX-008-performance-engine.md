# EX-008: Exodus Performance Engine

P0 measurement and stability; performance claims require paired real Switch measurements. Supports native HOS titles and locally executed Windows titles. Preserve stock fallback and safe thermal/power limits.

## Work packages
| ID | Work | Scope |
| --- | --- | --- |
| EPE-01 | Frame timing, CPU/GPU/RAM, clocks, temperature, power and memory pressure capture | Both |
| EPE-02 | Per-title adaptive resource governor atop existing 4IFIR controls; safe clocks and rollback | Both |
| EPE-03 | ZibBurger cold-page pool, actual resident savings, page restoration, GPU fence-aware staging | Both where possible |
| EPE-04 | FEX/Box64 profiling, NEON mapping, hot-block JIT, validated cache invalidation and native thunks | Windows |
| EPE-05 | Mesa/DXVK/VKD3D shader/pipeline cache, barrier profiling, GPU compression and zero-copy where supported | Windows |
| EPE-06 | Native game-specific FPSLocker/resolution profiles; no assumed universal hooks | Native HOS |
| EPE-07 | Resource streaming, SD I/O and bounded background services | Both |
| EPE-08 | Dynamic resolution and FSR/NIS feasibility; generation of frames only when measured beneficial | Both where supported |
| EPE-09 | Per-title AutoTune with rollback, user profiles and EX-007 UI | Both |

## First implementation
Run from the repository root with Python 3.8+:
```sh
python -m exodus.performance baseline.txt candidate.txt
python -m unittest discover -s tests -p test_performance.py -v
```
Input files contain one positive finite frame duration in milliseconds per line. At least 100 and at most one million frames per trace; allowed durations 0.001–60000 ms. Reports mean FPS (1000/mean duration), 1% low FPS (1000/mean of slowest ceil(1% of frames)), nearest-rank P99 frame time and relative changes. Negative P99 change is favorable. The 1% definition is explicit; other tools may differ. This comparator is offline, not an on-device capture tool.

## Acceptance
- Pin firmware, console revision, game/version, runtime, save/scene, resolution, FPS cap, dock/handheld, clocks, ambient conditions and trace source. Capture warmup separately, use repeated matched runs and report variability. Do not compare generated frames with native frames as though equivalent.
- Include 1% low, P99, average FPS, RAM peak/residency, thermal and power metrics, stability, input latency where measured. No hardware claims based on synthetic tests.
- Baseline and optimization must preserve correct output; crashes, memory corruption, degraded controls, unsafe temperatures or power regressions block default enablement.
- Independent review, reproducible builds and actual hardware logs before declaring work complete. Existing tools may be reused after source and license verification.
- EX-005 sparse codec is only a primitive; EX-006 Steam and EX-007 Control Center have separate acceptance gates.

References: https://github.com/autorunhq/autorun ; https://github.com/danfromtico/mesa-switch ; https://github.com/masagrator/FPSLocker ; https://github.com/rashevskyv/4IFIR

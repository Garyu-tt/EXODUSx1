# ADSP native service contract — reference stage 1

Scope: EXODUS-owned lifecycle policy for Switch 1 only. Not a Nintendo ADSP firmware wire ABI, driver, audio engine, or physical admission. The approved AMP1 transport is unchanged. No firmware blobs, registers or device writes.

## Evidence inventory

* [Tegra210 Kconfig](https://kernel.googlesource.com/pub/scm/linux/kernel/git/will/linux/+/b68b9907069a8d3a65bc16a35360bf8f8603c8fa/drivers/soc/tegra/Kconfig): Cortex-A9 ADSP is an audio processing resource; exact board admission is independent.
* [Switchbrew audio services, revision 15612](https://switchbrew.org/w/index.php?title=Audio_services&oldid=15612): public HOS renderer API and reported internal command generation, events and fault reporting. Public IPC is not the ADSP firmware command encoding.
* [libnx audren interface](https://switchbrew.github.io/libnx/audren_8h.html): public renderer API corroboration.
* [Switchbrew service registry](https://switchbrew.org/wiki/Services_API): hwopus service entry is Switch 2-specific; investigate Switch 1 Opus separately, do not infer physical offload.

| Service responsibility | Evidence | Target requirement |
| --- | --- | --- |
| Renderer configuration, work buffer and behavior revision | HOS API | Exact supported board/HOS version; preserve HOS validation |
| Renderer start/stop, update, completion notification | HOS API; reported ADSP command generation | Preserve update/event semantics; no completion before actual backend result |
| Mixing, voice processing, effects, sample conversion | Renderer feature inventory only | Audit exact native command coverage and golden vectors |
| Audio output, input, routing and device changes | Public HOS APIs | Determine which responsibilities actually execute on ADSP; retain single hardware owner |
| Opus | Separate HOS service investigation | Determine actual S1 firmware participation; no S2 assumptions |
| DMA, IRQ, cache, shared buffers | Hardware dependencies, board specifics unknown | Exclusive ownership, validated bounds and visibility; no raw client pointers |
| Suspend/resume, watchdog, exception reporting | HOS behavior and reported ADSP fault path | Preserve error reporting, bounded recovery and audio continuity; no watchdog suppression |

## Reference contract

The isolated C fixture models READY → RUNNING → READY → SUSPENDED → READY,
one outstanding operation, monotonic generation-scoped IDs, validated offset/length,
completion matched to generation and ID, a bounded externally supplied monotonic
deadline, and terminal FAULT on timeout/backend failure/clock regression. Stop or
suspend requires drain. Reinitialization requires an external quiescence proof;
the fixture does not implement that proof. Four-byte alignment and pool size are
synthetic policy, not hardware ABI. No audio data is processed and no physical
success can be inferred. No coupling to AMP1 until separate opcode/version and
authorization review. The kernel-only ADSP target still requires a genuine A57
execution substrate for BSD user applications.

## Hardware admission remains blocked

Record exact Mariko/Erista model and HOS build, firmware task ABI and revisions,
memory and cache behavior, IRQ/DMA resources, clock/reset ownership, scheduler and
timing budgets from authorized evidence. Compare stock and candidate audio under
load (underruns, latency, jitter, output continuity, suspend/resume and recovery)
while independently proving real FreeBSD and Horizon liveness. If original
firmware needs exclusive CPU/resources, stop physical admission and evaluate a
compatible implementation; no claim that firmware can run unchanged.

Refs #103 #105 #349 #350. No change to physical graph states.

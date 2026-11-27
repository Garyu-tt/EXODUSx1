# Tumor HOS Probe R03

One normal homebrew application run collects the OS version, process memory
budget, allowed core mask, a finite arithmetic task on each permitted A57
core, a two-pattern check of 1 MiB of its own allocated memory, monotonic
user timer progress, and audio-service connection results. The app creates
a new TUMORH03.TXT without overwriting an existing report.

Run under the existing homebrew launcher; PLUS exits. No bootloader changes,
firmware changes, raw registers, elevated access, or external memory access.
Audio service availability is not proof of ADSP execution. Process memory
is not physical RAM capacity; core restrictions are process permissions,
not proof of disabled hardware. No claims about EL2 or simultaneous BSD.
Build CI produces an NRO; physical execution must be observed separately.

R02 additionally launches permitted cores together, checks arithmetic and
release/acquire peer data publication, records worker intervals and times
four 4 MiB application-owned buffer copies. Interval overlap is not proof
of continuous concurrent execution; copy timing is effective copy throughput,
not raw DRAM bus bandwidth. No privileged accesses or clock changes.

R03 adds silent 48 kHz PCM renderer sessions with 1, 4 and 8 voices,
500 ms buffers, frame event counts, completed buffers, voice drops and
elapsed time. Frame waits are bounded; ordinary service IPC itself is
not forcibly timed out. No custom ADSP code, firmware changes or proof of
ADSP execution. Public API reference: https://switchbrew.github.io/libnx/driver_8h.html

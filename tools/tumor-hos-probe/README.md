# Tumor HOS Probe R01

One normal homebrew application run collects the OS version, process memory
budget, allowed core mask, a finite arithmetic task on each permitted A57
core, a two-pattern check of 1 MiB of its own allocated memory, monotonic
user timer progress, and audio-service connection results. The app creates
a new TUMORH01.TXT without overwriting an existing report.

Run under the existing homebrew launcher; PLUS exits. No bootloader changes,
firmware changes, raw registers, elevated access, or external memory access.
Audio service availability is not proof of ADSP execution. Process memory
is not physical RAM capacity; core restrictions are process permissions,
not proof of disabled hardware. No claims about EL2 or simultaneous BSD.
Build CI produces an NRO; physical execution must be observed separately.

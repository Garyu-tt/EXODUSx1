# R16 physical admission: evidence before ADSP deployment

R15.14 #383: 13/13 PASS, native ARMv7 kernel + A57 clone in two QEMU
systems; R15.15 #384 and R15.16 #385: 7/7 PASS each. These do not
establish board support, physical coherency or access to EL2 on Switch.

## Donor research
- [NVIDIA-authored Tegra210 audio driver, pinned blob 24c6affffc5931ce2b5ddf116720e4bbcaf2546b](https://chromium.googlesource.com/chromiumos/third_party/kernel/+/firmware-strago-7287.B-chromeos-3.18/sound/soc/tegra-alt/tegra210_adsp_alt.c):
  donor uses ADSP OS loading, app loading, shared DMA buffers, mailboxes,
  runtime power management. This is a Linux host driver, not Horizon ABI.
- [Switchbrew audio services](https://switchbrew.org/wiki/Audio_services):
  separate renderer, output and hardware Opus service contracts. Public
  interface descriptions are not proof of an internal compatible firmware.
- [Switch security slides](https://switchbrew.github.io/34c3-slides/):
  identify Tegra ADSP as Cortex-A9. Not board-specific feature evidence.

## Gate
Use tegra_physical_gate.py with operator-collected manifest and hashed
artifacts. Twelve independent gates, same explicit board/SoC identity.
QEMU/source-only claims cannot pass. Even all matching artifacts yield
MANUAL_REVIEW_REQUIRED, not approval. Hashes prove file integrity, not
provenance. No register access or hardware changes in this tool.
No assumed ADSP boot address, MMU, timer, RAM allocation or reset path.
Preserve audio, DMA ownership and suspend/recovery. If no authorized
JediControl transport is available, record BLOCKED, not hardware failure.
A future target probe must be separately reviewed and authorized.

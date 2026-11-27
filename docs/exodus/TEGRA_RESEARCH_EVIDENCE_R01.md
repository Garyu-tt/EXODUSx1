# Tegra research evidence R01

Date: 2026-11-27. Scope: existing Tumor architecture (#103, #105, #349). Documentation only; no new product architecture, device access, deployment, or physical acceptance. Public source evidence is not equivalent to verification on the owner's device.

## Source ledger

| ID | Public source | Observed evidence | Applicability / unresolved |
| --- | --- | --- | --- |
| T01 | [FreeBSD e903478919602c90fdc202a8628b89eb7c3bc104](https://cgit.FreeBSD.org/src/commit/?id=e903478919602c90fdc202a8628b89eb7c3bc104) ([commit notice](https://lists.freebsd.org/pipermail/dev-commits-src-main/2020-December/000169.html)) | TX1/Nano port: 18 files, 10,773 additions; MAX77620, CAR, PLL, PMC, pinmux, cpufreq, USB padctl | Historical SoC driver donors, not proven Switch board drivers or safe concurrent ownership. Audit current FreeBSD and license notices. |
| T02 | [ChromiumOS Tegra210 ADSP audio driver](https://chromium.googlesource.com/chromiumos/third_party/kernel/+/firmware-strago-7287.B-chromeos-3.18/sound/soc/tegra-alt/tegra210_adsp_alt.c) | tegra210_adsp_init calls nvadsp_os_load/start, nvadsp_app_load and nvadsp_os_suspend | Independent OS/app lifecycle in NVIDIA BSP; not a supported Horizon extension API. Full underlying loader not reviewed. |
| T03 | [Linux Tegra SoC Kconfig](https://kernel.googlesource.com/pub/scm/linux/kernel/git/arm64/linux/+/06898e5320b5c12dd8c21d0ee8d5a68347001746/drivers/soc/tegra/Kconfig) and [34C3 Switch architecture](https://switchbrew.github.io/34c3-slides/) | ADSP Cortex-A9 / ARMv7 | ARM64 FreeBSD image cannot execute on this CPU. ARMv7 board port or a minimal supervisor plus AArch64 kernel remain distinct hypotheses. |
| T04 | [Linux Tegra210 MC source, blob f58f3ef6f68191ede7098bfa4dad546ce4de275d as observed](https://linux.googlesource.com/linux/kernel/git/bpf/bpf-next/+/refs/heads/master/drivers/memory/tegra/tegra210.c) | MC clients, SMMU groups, 128 ASIDs, 48 TLB lines, display grouping, 34-bit MC addresses | SoC driver declarations; no claim that all groups are accessible, independent or free under Horizon. CPU stage-2 does not by itself isolate device DMA. |
| T05 | [Switch board DTS reference mirror](https://notdouble.com/p/alarm-tegra-x1/file/reference/switchroot-mainline/tegra210-nintendo-switch.dts) | 4 GiB memory declaration and MAX77620 node; reserved-memory includes ramoops | Secondary source, confirm upstream revision and board variant before use. Not Horizon live allocation map. |
| T06 | [Switchroot Linux features](https://wiki.switchroot.org/wiki/linux/linux-features) | Erista/Mariko and multiple board variants supported; separate nvpmodel configuration | Alternative boot evidence only; distinguish revision and board. |
| T07 | [Switchbrew audio services](https://switchbrew.org/wiki/Audio_services) | Search index describes audio work memory and ADSP firmware changes | Direct page fetch returned HTTP 403 during this review; treat as pending verification, not a confirmed protocol contract. |

## Admission work (host first)

- A57/ADSP: verify ISA, ABI, word widths and address representability; shared records use explicit fixed-width serialization rather than native pointers. Validate malformed records, queue wrap, restart epochs and timeout behavior in existing fixtures.
- DMA/MMU: catalog clients and isolation groups from T04 and board sources; model exclusive owner, quiesce/fence, map/unmap, rollback, and reject unmapped access. CPU-only emulation cannot prove hardware coherency.
- Power/clocks: inventory FreeBSD T01 driver dependencies against board DTS; suppress unreviewed reset/clock/power writes in coexistence design. Verify resource ownership before enabling driver attach.
- Audio: document existing owner and resource lifecycle. Do not reset or replace active audio firmware based on T02 alone; keep audio continuity a physical gate.
- Physical test queue: only unanswered questions after source comparison, compile and host test. Batch board/revision confirmation, concurrent liveness, shared memory and cache/DMA, audio continuity, ownership handoff and recovery. Require rollback and separately recorded physical evidence.

## Status

Source inspection performed, not compilation or emulation. No hardware access. Source revisions and licenses need a complete audit before code reuse. No claim that stock Horizon permits the proposed ADSP or EL2 arrangement. Cross-reference existing #349 admission graph instead of creating a second graph.

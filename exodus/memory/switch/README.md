# EXODUS Page Probe for Nintendo Switch

Build with devkitPro's Switch toolchain and libnx. Run `make` from this directory in the devkitPro shell. Copy `exodus-page-probe.nro` to `/switch/exodus-page-probe/` on the SD card, then launch **EXODUS Page Probe** from hbmenu.

The probe starts by packing three generated 4096-byte pages into one in-memory group, restoring each by index and comparing the recovered bytes. The expected payload sizes are 8 bytes for an all-zero page, 72 bytes for one occupied block and 4096 bytes for a dense page. The group shows 4176 bytes of payload and 24 bytes of offsets. Press **A** to repeat.

To measure a file, place your sample at `/switch/exodus-page-probe/sample.bin` on the SD card. Press **X** to read it in batches of up to 128 pages. The screen shows the verified full pages, sparse pages, word-run pages, input size, encoded payload and the space needed for a two-byte length index per full page. A trailing fragment counts at its original size. Press **B** to cancel a scan or **+** to return to hbmenu. The probe opens the sample read-only and uses its own buffers. Results describe the selected data. Runtime RAM/VRAM residency requires separate measurement.

The demo checks the adaptive format on a solid RGBA page. It verifies both the expected 41-byte payload and the reconstructed 4096 bytes.

## EX-005 acceptance timing (QA only)

Press **Y** to run seven samples of 32,768 operations per case and write
`/switch/exodus-page-probe/ex005-baseline.csv` or `ex005-candidate.csv` (overwrites only the same variant).
The ten cases measure BDI direct reads, BDI fingerprint, cold patch preparation,
and exclusive hot slab writes on fully packed pages, plus direct reads and
fingerprints at 8, 32 and 56 packed blocks out of 64. Mixed pages use a fixed
dispersed mask and check every word against the original before measurement.
Expected mixed encoded lengths are 3753, 2697 and 1641 bytes. Each batch checks its result against a deterministic
oracle; failed or incomplete CSV files are invalid. The timer is the ARM system
counter, not the CPU cycle counter. Host builds use process CPU time and are
not comparable with Switch times. The probe reserves buffers, not OS RAM/VRAM.

For A/B use the same benchmark source, compiler, flags and probe UI on both
baseline and candidate, with separately identified binaries and the same console,
clock configuration, thermal conditions and workload. Preserve each order's CSVs externally
before running the next binary. Compare per-case median of seven ns/op samples,
check equal digests and fixture sizes, and repeat in reverse run order.
Record exact source SHAs, devkitA64/libnx versions, actual CPU clock and applet/title
mode. Do not equate timer ticks with CPU cycles or extrapolate to game FPS.
The baseline is `a000dba9cd71da1568134f6308bb67bc439f8506`; candidate is
`a06ab531edf4fa52e0f5c1cc93f1eb3641bafdb2`. Copy only the benchmark and probe
build/UI changes into a baseline worktree, not the candidate memory code.
Host smoke: `make -C exodus/memory bench-ex005`.

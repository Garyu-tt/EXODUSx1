# EX-005 physical Nintendo Switch evidence

Date: 2026-11-26. Production candidate a06ab531edf4fa52e0f5c1cc93f1eb3641bafdb2; baseline a000dba9cd71da1568134f6308bb67bc439f8506. QA build head 8b2635a34f4efc0f267b39b43ddffe50137ba8eb; actual candidate merge checkout 1cc735d7f7832e2219952d648eb96f293899f63f. Build artifact 15132372780, run 45849929417. devkitA64 r29.2-1 (GCC 15.2.0), libnx 4.12.0-1, switch-tools 1.13.1-1.

Four original CSVs are preserved byte-for-byte; see SHA256SUMS. Owner launched both NROs on physical hardware, in AB then BA order, and reported success. Reports were copied from SD using JediControl and checked against original hashes. Each file has 70 timing rows (seven trials per ten workloads) and six fixture metadata rows. All digests match across variants and orders. Timer 19.2 MHz is the ARM system timer, NOT CPU clock. Actual CPU clock, temperature and title/applet mode were not logged. Results are isolated memory microbenchmarks, not game FPS or OS RAM residency.

Reproduce from repository root:

```sh
(cd docs/exodus/evidence/ex005-2026-11-26 && sha256sum -c SHA256SUMS)
python exodus/memory/compare_ex005_bench.py docs/exodus/evidence/ex005-2026-11-26/device-ab/ex005-baseline.csv docs/exodus/evidence/ex005-2026-11-26/device-ab/ex005-candidate.csv
python exodus/memory/compare_ex005_bench.py docs/exodus/evidence/ex005-2026-11-26/device-ba/ex005-baseline.csv docs/exodus/evidence/ex005-2026-11-26/device-ba/ex005-candidate.csv
```

Both orders: 10/10 workloads faster, 9,175,040 measured operations total. Independent review remains required; no main admission by this evidence commit.

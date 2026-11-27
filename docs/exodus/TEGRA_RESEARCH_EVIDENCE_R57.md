# R57 — minimal kernel direct boot admission and memory comparison

R56 source build run 46048553785 job 140673832117 passed tool setup and source configuration, compiling at observation. No ELF size or build success claim yet.

R57 commit 6734b8130a03aac3f42c0139df2ac36910d3a56a creates tegra-freebsd-a9-minimal-boot.yml. An admission job verifies exact R56 head SHA 43722405848031e3fc828e1a1259fd84adb7f07b, waits with an eight-minute bound for successful completion, then admits 24/28/32/64 MiB matrix. Downloads exact build run artifact, verifies kernel SHA against that artifact's inventory (integrity check, not independently signed provenance), confirms EXODUS_A9_MIN config, uses R54 tested ELF load planner and derives physical entry from ELF. Same QEMU vexpress-a9, single core, memory placement, verified root image snapshot, single-user shell gates and no additional maxusers tuning. Requires reported hw.realmem exactly equal to requested RAM. Captures serial, VM counters, kernel digest, DT and load plan. Baseline remains 24/28 FAIL, 32/64 PASS.

CI run 46048811743 in progress at observation; all new boot results pending. If admission fails, inspect original source build before rerun; do not treat skipped matrix as a failed boot. No physical Switch access or physical ADSP validation.

CI: https://github.com/Garyu-tt/EXODUSx1/actions/runs/46048811743

# R08: four-core EL2 SMP reference
QEMU virt, four Cortex-A57 vCPUs. Primary starts three secondary CPUs via the
reference PSCI SMC CPU_ON conduit; each validates affinity and EL2 entry,
uses its own stack, stage-2 translation root and VMID, and maps identical
guest IPAs to distinct writable physical pages. Shared read-only guest code.
Acquire/release readiness barrier before guest entry. Each guest completes
64 HVC transactions with its own progress and identity; primary reports only
after acquiring four success flags. CI requires 64 actual HVC exceptions per
CPU, 256 total, and the EL1-only negative control. Host watchdog bounds hangs.
No core is reserved solely for monitoring in this fixture.

This does not validate target PSCI firmware, GIC, cache/coherence, SMP guest
kernel, CPU hotplug, vCPU migration, timer scheduling across cores, full R07
state integration, Horizon, FreeBSD or physical Switch. Reference stage-1
translation is off; memory sharing is QEMU-only. R08 is an independent
execution gate, not evidence that previous component tests are integrated.
Refs #103 #105 #354 #359.

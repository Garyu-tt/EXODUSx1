# EXODUS NX JIT probe

Requires devkitPro, devkitA64 and libnx:

    make -C exodus/nxposix/jit_probe

Copy the resulting NRO to the Switch and launch it in a homebrew environment. It prints HOS version, process handle, syscall hint availability, libnx JIT backend, and RW/RX aliases. It writes a two-instruction AArch64 function into the writable alias, transitions through libnx JIT API, executes it through the executable alias, checks that it returns 42, then closes the JIT object.

Exit code 77 means libnx reports JIT unavailable; this is an expected capability result, not a crash. Exit 1 means a JIT operation, execution result, or cleanup failed.

This test executes only a constant-return instruction sequence; it does not access game memory, patch Horizon, alter process permissions directly, or install a kernel module. Run on emuMMC first. Record exact HOS, Atmosphere, 4IFIR and loader versions. Do not infer that syscall hints exist merely because the firmware is recent.

Not built or run here: the current environment has no devkitA64/devkitPro toolchain or Switch hardware. Source-level implementation only.

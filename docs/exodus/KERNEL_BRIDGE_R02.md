# Kernel Bridge R02: native admission probe

Stacked on #30 (R01 graph). This is an own-process diagnostic, not a privilege escalation or process launcher. It neither alters firmware nor accesses other processes. The Switch path uses libnx's existing console and input helpers. Its success demonstrates only the four individual query operations in this execution context.

## Build and run
Host: `cc -std=c11 -Wall -Wextra -Werror -pedantic tools/kernel_bridge/admission.c -o /tmp/exodus-admission && /tmp/exodus-admission`.
Switch: use a standard devkitPro libnx application template, replace its source with `tools/kernel_bridge/admission.c`, build and launch the NRO in a supported homebrew environment. Pin and record libnx/toolchain revisions, firmware and applet/application launch mode; do not claim cross-build or physical-device success until performed.

The console reports HOS version encoding, applet type, own PID/TID, memory limit and memory use with the original Result for every query. Zero is valid; a failed query is unknown and never treated as zero. Limit and use are per-process observations, not total physical RAM or an atomic snapshot. The result never promotes the entire graph to device_verified.

## Sources
- https://switchbrew.github.io/libnx/svc_8h.html (svcGetProcessId, svcGetThreadId, svcGetInfo and InfoType)
- https://switchbrew.github.io/libnx/hosversion_8h.html
- https://switchbrew.github.io/libnx/applet_8h.html
- https://github.com/switchbrew/libnx

## Next gate
Add separately bounded service admission evidence; avoid synchronous service discovery that can wait indefinitely. A service session is not proof of all commands. Then implement a real process lifecycle provider with explicit permissions, supervised two-program test and clean exit. No fake PIDs, no assumption of arbitrary spawn or fork.

Host assertions are provided and CI is configured. At submission neither host CI nor cross-build/device execution has been observed.

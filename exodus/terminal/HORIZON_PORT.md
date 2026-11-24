# NX-BSD Horizon port admission

Host baseline: CI run 45478499082 passed 6 integration tests each for full and NO_HISTORY x86-64 Linux executables. ARM64 Linux execution is a separate architecture gate, not proof of Horizon ABI compatibility.

The existing bridges are unmerged candidates, not device-proven implementations.

| Required shell capability | Existing candidate | Remaining gate |
|---|---|---|
| Current process identity | PR #32, kernel_bridge_horizon.h at 4ae635389052c45c4e32a26622e539247945ed7d | Validate PID range and error translation on target |
| Process launch and wait | PR #33, process.h at 86cc79c94a609a4e61a622b2d32c83b015d24c84 | Native provider, argv/env, blocking wait, exit status; current interface only spawn(program)/poll |
| fork/vfork, subshell, execve | No equivalent admitted | Explicit execution model; do not equate NRO handoff or a thread with fork |
| Socket readiness and handle wait | PR #19 at e77fca1489217c5924b0238e953213ba385438d4 | Native probe; file descriptors and Horizon handles are distinct |
| pipe, dup2, redirections | No integrated provider | Test descriptor isolation, close semantics, backpressure, EOF |
| Signals, process groups, terminal sessions | No integrated provider | Implement shell-owned delivery and terminal lifecycle, no fake success |
| libc / file paths / stat / dirent | libnx/newlib candidate | Compile/link and SD filesystem tests; keep mount and security boundaries |
| fwopen, setmode, getmode, strlcpy | Host libbsd/glibc only | Native implementations or licensed donor reuse, contract tests |
| O_VERIFY | Unsupported on current host adapter | Refuse verified opens until a genuine verifier is available |
| libedit / PTY | Linux reference only | Target terminal backend and interactive tests |

Admission sequence: native AArch64 Linux reference, then devkitA64 compile/link inventory, then isolated native provider probes, then incremental shell features. No device process privileges or firmware modifications are presumed. A failed provider must not execute the requested command or report success. The existing NRO terminal remains a separate recovery interface.

R19 expands the host shell suite with async wait, command substitution, function scope, descriptor duplication and EXIT traps. These new tests and the ARM64 job are pending at the time of this document.

## R20–R21 evidence
CI run 45478893708 passed ARM64 Linux headless and x86-64 full/headless profiles with 11 runtime tests per profile. Native SDK run 45479150192 built exodus-terminal.nro and inventoried 42 symbols. Not linkable: pipe, waitpid, vfork, sigaction, sigprocmask, setsid, setpgid, tcgetpgrp, tcsetpgrp and other credential/resource APIs. fork, execve and kill link but remain unverified; inspect implementation symbols before any invocation. The R21 SDK inventory now adds stream and signal primitives and selected disassembly. Use the terminal's explicit read-only `abi` command on the device to validate identity, monotonic clock, cwd stat and directory reading. No process creation, signal delivery or SD writes are tested. R21 results are pending.

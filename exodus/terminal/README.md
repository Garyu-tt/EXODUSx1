# EXODUS Terminal R04: NX-BSD (P0)

A native libnx text console, independent of desktop graphics and optional GPU Relay. Build with devkitA64/libnx: make -C exodus/terminal. Copy the resulting NRO to the SD homebrew directory and launch through the compatible loader. Use A to enter a command via software keyboard, Plus to exit. No firmware changes or persistent installation required.

Commands: help, pwd, ls [path], cd path, cat path, stat path, basename path, dirname path, echo text, id, mem, ps, svc, apps, nro path, run NAME, status, clear, exit. Filesystem inspection is read-only; no arbitrary executable launch; only the explicit NRO handoff aliases below. Output is bounded and non-ASCII/control bytes escaped, not a full Unicode/VT emulator. A path with spaces is accepted as the entire argument. No shell expansion, pipes or redirects.

Host tests: python3 -m unittest discover -s exodus/terminal -v (GCC/Clang on POSIX). Native build and actual Switch execution are required before claiming device support, including the owner's target HOS 4.9 (current SDK and firmware compatibility not assumed).

## P0 sequence
1. Native local terminal and filesystem commands, hardware build and smoke tests.
2. Connect Kernel Bridge shell #35 and its process/service provider #32–34; no duplicate lifecycle manager. Implement approved NRO launch and safe return-to-shell before general process semantics.
3. Share a terminal line discipline between local console and SSH #37; USB keyboard/controller text input, scrollback, bounded UTF-8/VT parser.
4. Interactive SSH PTY, nonblocking read/write/cancellation, strict host key verification, key auth, SFTP. Optional SSH server is opt-in, least privilege, no default network listener.
5. VFS write operations, package tools, diagnostics, network management and desktop terminal frontend.

GPU Relay #41 and desktop/compositor enhancements are secondary to a usable text-first environment. Keep original titles and recovery path working. Host test success cannot substitute for device testing.

## R02 diagnostics
`id` observes own PID/TID; `mem` queries own-process memory limit and usage via the read-only SVCs documented in #31. It is not global RAM or VRAM accounting. Failed queries preserve native error codes. `stat` reports file type and size. The temporary diagnostic adapter will be consolidated with Kernel Bridge #32 after that stack is integrated. `sdmc:/` paths are now accepted; other explicit device prefixes remain rejected.

R01 GitHub Actions succeeded (run 45470486486). R02 CI pending at this update; no device build or execution verified.

## R03 application handoff
- `apps`: lists known aliases, not an installed application inventory.
- `nro path`: bounded structural preflight of NRO header, image and segment extents. Does not verify signatures, code safety or compatibility. Temporary adapter to be consolidated with #11.
- `run ssh`: schedules handoff to `sdmc:/switch/exodus-ssh.nro`.
- `run autorun`: schedules handoff to `sdmc:/switch/wine/wine-nx-runtime.nro`.
- Only explicit user commands, application mode, compatible loader and successful preflight permit handoff. On successful scheduling the terminal exits. Return to terminal, app exit status, background processes and arbitrary argv are not supported yet. Host build never launches these apps.
- `ps` reports only the current process; `svc` explicitly reports unavailable supervisor state. Integrate #32–35 after shared interfaces land rather than creating another process manager.

R02 CI passed: https://github.com/Garyu-tt/EXODUSx1/actions/runs/45470950263. R03 host tests added; native build, HOS 4.9 compatibility and actual loader handoff remain unverified.

## R04 FreeBSD userland direction
Use freebsd/freebsd-src as a source donor, not as a replacement kernel or binary ABI. Imported `lib/libc/gen/basename.c` and `dirname.c` into `bsd_path.h` with original Nuxi copyright and BSD license text, private symbols and portability-only linkage/cast changes; original blob IDs are in the header. The terminal exposes the single literal argument subset, not full utility flags or POSIX shell parsing. R03 host CI succeeded: https://github.com/Garyu-tt/EXODUSx1/actions/runs/45471144070. R04 host and native checks pending.

P0 follow-up: import portable FreeBSD utilities as reviewed source slices, then port actual bin/sh and libedit. Map process spawn/wait, pipes, file descriptor redirection, signals, termios and PTY to NX-POSIX/Kernel Bridge (#19, #32–35); unsupported contracts must fail explicitly. Do not pretend Capsicum, fork or FreeBSD syscalls are available, or silently bypass sandbox checks. Keep source provenance and upstream licenses, pin a verified fork commit before reproducible donor imports. Preserve native title execution. FreeBSD binary compatibility is separate and not assumed.

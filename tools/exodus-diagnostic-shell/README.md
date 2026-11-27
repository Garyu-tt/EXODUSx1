# EXODUS Diagnostic Shell R01
Install the NRO once. Open in foreground and press A to enable a single session.
On the same trusted isolated LAN run python client.py SWITCH_IP and enter the
one-time code displayed on the console. Commands: info, cpu, memory, timer,
audio, all, quit. Reports are collected on the host. PLUS closes an idle
session; finite workload completes before input is checked again.
No arbitrary shell commands, file access, code uploads, privilege changes,
background listener, firmware changes or network exposure. Plain TCP, no
confidentiality or protection from an active LAN adversary: use isolated LAN
only. One pairing attempt per launch, one client, five-minute idle timeout,
100 commands max. Native functions require a new binary; existing tests can
repeat within a session. Ordinary service IPC is not forcibly timed out.
No ADSP or EL2 acceptance claim. Physical networking and repeated tests pending.

## PC client R02 (same NRO)
Use `help` or `repeat all 3`, `repeat cpu 5`, etc. At most 10 tests per batch and 100 server commands per session. Validates framing and report identity, verifies existing digest-named files, prints min/median/max for successful measurements, stops on repeat_safe=no. No automatic re-pairing. Replace the PC script only; the current session keeps using its original client until it exits.

## R03 programmable diagnostic VM
The new NRO accepts a numeric instruction sequence: `vm 0x1 0 0o377 1 1 2 2 0 1 0` returns r0=257. Decimal, 0x, 0o and 0b can mix. See diag_vm.h for opcodes. Each request uses a fresh 8-register, 64-word scratch VM, at most 63 non-END instructions; unsigned arithmetic wraps modulo 2^64. Invalid programs yield an error and no published register state. No native execution, addresses, MMIO, arbitrary files or privilege transitions. Programs update per request; replacing native functions still requires a controlled NRO replacement. No JediControl changes or bypass. The PC R03 client can still issue the previous diagnostics, but VM commands need R03 NRO. Build and physical testing pending.

## R04 local approval (no code)
Launch NRO and press A to listen. Start client.py on the PC. The Switch displays the connecting PC IP; press A to approve that address or PLUS to reject. Approval expires in 30 seconds. No typing or transmitting a pairing code for protocol 2; only one pending connection, no commands accepted before approval. The client still supports older protocol 1. Plain TCP on a trusted isolated LAN only, no cryptographic peer identity. No JediControl changes. R04 requires one NRO and client replacement.

## R05 workspace and diagnostic language
Protocol 3, same local approval. RAM-only flat VFS: 16 named files, 768 printable ASCII characters each, lost when NRO exits. Commands: ls, put NAME SOURCE (create/replace), cat NAME, run NAME, rm NAME. Names are alphanumeric, underscore or hyphen. Example: `put bench r0=core_mask(); r1=0o7; r0*=r1;`, then `run bench`. C-like subset: 8 unsigned 64-bit registers, literals, register references, = += -= *= ^= &= |=, semicolon terminators, up to 64 statements, no loops or native pointers. Read-only real Horizon probes: core(), core_mask(), total_memory(), used_memory(), ticks(). No filesystem outside workspace, arbitrary native execution or privilege access. Failed scripts publish no register results. Existing VM and fixed diagnostics remain. PC and NRO replacement required once. CI and physical acceptance pending.

## R06 bounded loops and scratch arrays
`repeat 16 { m[r0]=r0; m[r0]*=r0; r0+=1; }` with r0 initially zero. Use m[15] to read the result into a register. 256 unsigned 64-bit words reset each execution, literal/register index, bounds checking. Loop counts 1..1024, nesting depth 4, shared 4096 fuel units across statements and loop iterations. Division and remainder reject zero. tick_hz() returns the system counter conversion frequency. PC command `repeat run NAME 5` repeats a stored script and summarizes successful execution durations. No new privilege, native execution, or real filesystem. R05 programs remain compatible within the new bounded grammar. Physical verification pending.

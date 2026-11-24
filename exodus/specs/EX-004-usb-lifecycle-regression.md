# EX-004: USB/MTP lifecycle regression (P0)

Owner report 2026-11-24 (not independently reproduced): direct USB connection to laptop Thunderbolt port and connection through adapter on that port fail on latest installed 4IFIR. Same connection type works in Hekate. After attempted Thunderbolt connection, ordinary computer USB connection also stops working. Several restarts and laptop restart did not immediately restore it. Later a lock/connect/unlock attempt produced a black screen / unlock failure; recovery was observed around that sequence, not proven causal. Exact HOS/4IFIR builds, Switch model, cable connector types, Windows build, and failure stage unknown. Do not assume USB-C vs USB-A or exact link speed.

## Diagnostic matrix
- A: known-working ordinary USB port, awake, idle.
- B: Thunderbolt/USB-C port direct with compliant known-good data cable; both connector orientations if applicable.
- C: same port through known-good adapter, record adapter model separately.
- D: same physical connections in Hekate USB tools, record actual USB mode and VID/PID separately. Hekate USB mass storage and Horizon MTP are different protocols; a working Hekate path is a useful physical control, not proof of MTP.
- E: ordinary port immediately after B/C failure, before rebooting. Repeat after Switch sleep/wake, and after Switch and host restarts separately. Record which action recovers the connection.
- F: connect while asleep then wake; unplug while asleep then wake; repeated awake/idle reconnects. Stop if black screen, overheating, or unsafe state; save crash reports after recovery. No deliberate interruption of active writes.
- G: transfer only disposable files; compare SHA-256 on original and returned copy after stable enumeration. Keep saves and NAND untouched.

## Host probe
On Windows, identify actual VID/PID in Device Manager during working connection. Run separately for each protocol if IDs differ:

    python exodus/mtp_probe.py --usb-id 1234:abcd --observe 120

Replace example IDs. Record events even from an absent/unready initial state. Cross-port instance changes are allowed and indicated, not mistaken for a failed same-port reconnect. An observation exit code of zero means the observation window completed, NOT that USB or MTP worked. The host probe is polling, may miss brief changes, and cannot see a new VID/PID without separate configuration. WPD readiness is not proof of a file transfer. No private device instance IDs in JSON.

## Device investigation
Identify actual Latitude MTP backend and ownership of USB interface before modifying firmware. Capture HOS/Atmosphere reports and MTP service logs with monotonic timestamps; distinguish USB role/attach, enumeration, interface setup, transport session, suspend/resume and teardown. Review cancellation, outstanding transfer cleanup, interface release, idempotent init, resume after failed attach and conflicts with other legitimate USB services. Hypotheses only, not established root cause. Recovery must not reset USB during a write. Do not replace Hekate or patch drivers blindly.

## Acceptance
On real device, each supported cable/port combination passes repeated enumeration and hash-checked transfer; at least 100 reconnect cycles across the matrix, including 20 sleep/wake cycles and 20 ordinary-port attempts after a failed or disconnected alternative connection. No stuck state, black screen, reboot requirement, data corruption or unexpected service conflict. Record unsupported configurations explicitly. No closure based solely on mocked Windows data. Do not use noncompliant passive USB-A-to-A cables.

Depends on PR #12; stacked branch for independent review. No console access or real hardware test performed.

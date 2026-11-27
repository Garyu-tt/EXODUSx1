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

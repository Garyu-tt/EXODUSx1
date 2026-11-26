# R15.11 live native child polling
Eight registered native children stay alive on private pipes during AWB1
boot. Each A57 EL0 makes a first exact-PID WNOHANG poll. Native kernel
returns PID 0; supervisor independently checks native WNOHANG, releases
and reaps that child before forwarding the unchanged zero response.
Second poll obtains the real PID and status from process_exit metadata.
Third poll returns ECHILD. EL0 checks untouched status on zero/ECHILD,
valid status on success and EFAULT for RX pointer. 24 native wire replies,
24 replay rejects, 40 SVC, 32 HVC, 512 IRQ expected. The release is a
deterministic test harness action, not a generic signal/fork syscall.
Fixed eight sessions, no blocking wait or physical Tegra. CI pending.

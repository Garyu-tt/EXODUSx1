# R15.9 live A57 to native FreeBSD AWT1
Two simultaneous QEMU systems, no synthetic peer. Native ARMv7 supervisor
forks eight children and reaps them before A57 starts; kernel exit hook
retains actual PID and status. Eight distinct AWT sessions map one-to-one
to child slots under a single authenticated native parent. A57 EL0 SVC7
uses fixed selector 2001..2008 (NOT native PID input), rejects RX destination,
returns actual native PID and copies status to private RW page. Native
parent checks each reply against native waitpid, rejects replay, forwards
kernel bytes unchanged. Host checks identity, distinct PID and status.
Limitations: fixed selectors, children already exited, shared supervisor
rather than eight native parents, no real remote fork, live polling or
generic rusage/copyout. Native kernel metadata reap only. No physical ADSP.
R15.5 fix remains separate pending CI. Fresh gate required.

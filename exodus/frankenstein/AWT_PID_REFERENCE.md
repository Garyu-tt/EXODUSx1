# R15.10 native PID provisioning
No fixed selector 2001..2008 in the new fixture. Native kernel supplies
64-byte AWB1 boot snapshot (version/count/generation/eight native PIDs/
reserved zero bytes) after children exit. Only the registered parent may
read it. Host forwards unchanged after monitor UART ready. Primary validates
generation, count, uniqueness, range and reserved fields and provisions each
private ELF RW page before secondary release. EL0 loads its actual PID and
uses it for wait4; EL2 verifies exact PID, returns only matching kernel
reply, copies status to its private cell. Fixture checks actual return PID.
Fixed eight slots and one trusted supervisor remain. No live polling,
remote fork, generic wait4 or physical ADSP. Boot message is trusted
fixture transport, not cryptographically authenticated or production IPC.
Fresh CI required. Parent #378 remains independently pending.

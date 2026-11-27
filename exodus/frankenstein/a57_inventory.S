/* SPDX-License-Identifier: GPL-2.0-only */
/* Caller-owned, aligned 48-byte record; privileged diagnostic context.
 * No entry/EL transition, no device MMIO, no cache or control changes.
 * Caller must handle traps and ensure memory ownership and visibility. */
.text
.p2align 2
.global tumor_a57_inventory
.type tumor_a57_inventory, %function
tumor_a57_inventory:
    str xzr, [x0, #40]
    mrs x1, CurrentEL
    str x1, [x0, #0]
    mrs x1, MIDR_EL1
    str x1, [x0, #8]
    mrs x1, MPIDR_EL1
    str x1, [x0, #16]
    mrs x1, ID_AA64PFR0_EL1
    str x1, [x0, #24]
    mrs x1, ID_AA64MMFR0_EL1
    str x1, [x0, #32]
    dsb sy
    mov w1, #0x4157
    str x1, [x0, #40]
    dsb sy
    ret
.size tumor_a57_inventory, .-tumor_a57_inventory
.section .note.GNU-stack,"",%progbits

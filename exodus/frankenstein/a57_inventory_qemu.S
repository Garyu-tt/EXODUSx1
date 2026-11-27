/* SPDX-License-Identifier: GPL-2.0-only */
/* QEMU virt only. No device accesses or privilege transitions. */
.text
.global _start
_start:
    adr x19, record
    ldr x20, =0x1122334455667788
    stur x20, [x19, #-8]
    str x20, [x19, #48]
    mov x21, #2
again:
    mov x0, x19
    bl tumor_a57_inventory
    ldr x2, [x19, #40]
    mov x3, #0x4157
    cmp x2, x3
    b.ne fail
    mrs x3, CurrentEL
    ldr x2, [x19, #0]
    cmp x2, x3
    b.ne fail
    mrs x3, MIDR_EL1
    ldr x2, [x19, #8]
    cmp x2, x3
    b.ne fail
    mrs x3, MPIDR_EL1
    ldr x2, [x19, #16]
    cmp x2, x3
    b.ne fail
    mrs x3, ID_AA64PFR0_EL1
    ldr x2, [x19, #24]
    cmp x2, x3
    b.ne fail
    mrs x3, ID_AA64MMFR0_EL1
    ldr x2, [x19, #32]
    cmp x2, x3
    b.ne fail
    ldur x2, [x19, #-8]
    cmp x2, x20
    b.ne fail
    ldr x2, [x19, #48]
    cmp x2, x20
    b.ne fail
    stp x20, x20, [x19, #0]
    stp x20, x20, [x19, #16]
    stp x20, x20, [x19, #32]
    subs x21, x21, #1
    b.ne again
    mov x2, #0
    b finish
fail:
    mov x2, #1
finish:
    adr x1, exit_args
    mov x3, #0x26
    movk x3, #2, lsl #16
    stp x3, x2, [x1]
    mov x0, #0x20
    hlt #0xf000
1:  b 1b
.ltorg
.data
.balign 16
.quad 0
.quad 0
record: .space 48
.quad 0
.balign 16
exit_args: .space 16
.section .note.GNU-stack,"",%progbits

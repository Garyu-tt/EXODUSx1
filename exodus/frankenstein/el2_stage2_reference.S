/* QEMU virt reference: one EL1 guest, stage-2 only. No target deployment. */
.section .text.boot,"ax"
.global _start
_start:
    mrs x0, CurrentEL
    cmp x0, #8
    b.ne blocked
    msr daifset, #15
    msr spsel, #1
    adr x0, stack_top
    mov sp, x0
    adr x0, vectors
    msr vbar_el2, x0
    /* Clear three 4KiB tables explicitly. */
    adr x0, l1
    mov x1, #1536
1:  str xzr, [x0], #8
    subs x1, x1, #1
    b.ne 1b
    adr x0, l1
    adr x1, l2
    orr x1, x1, #3
    str x1, [x0, #8]
    adr x0, l2
    adr x1, l3
    orr x1, x1, #3
    str x1, [x0]
    adr x0, l3
    adr x1, guest
    /* Valid page, normal WB, inner shareable, AF, read-only. */
    mov x2, #0x77f
    orr x1, x1, x2
    str x1, [x0, #8]
    adr x1, guest_data
    /* Read/write, XN. */
    mov x2, #0x7ff
    orr x1, x1, x2
    orr x1, x1, #(1 << 54)
    str x1, [x0, #16]
    adr x0, guest_data
    str xzr, [x0]
    adr x0, phase
    str xzr, [x0]
    adr x0, secret
    mov x1, #0x5a5a
    str x1, [x0]
    dsb sy
    /* 4KiB granule, L1 start, 39-bit IPA, 32-bit PA, WBWA inner shareable. */
    ldr x0, =0x80003559
    msr vtcr_el2, x0
    adr x0, l1
    msr vttbr_el2, x0
    isb
    tlbi vmalls12e1is
    dsb ish
    isb
    /* AArch64 EL1, stage-2 enabled, TGE off. */
    ldr x0, =0x80000001
    msr hcr_el2, x0
    ldr x0, =0x30d00800
    msr sctlr_el1, x0
    isb
    ldr x0, =0x40001000
    msr elr_el2, x0
    mov x0, #0x3c5
    msr spsr_el2, x0
    eret
    .ltorg

.balign 2048
vectors:
.rept 8
    b fail
    .balign 128
.endr
    b lower_sync
    .balign 128
.rept 7
    b fail
    .balign 128
.endr

lower_sync:
    mrs x9, spsr_el2
    and x9, x9, #15
    cmp x9, #5
    b.ne fail
    mrs x9, esr_el2
    lsr x10, x9, #26
    cmp x10, #0x24
    b.eq abort_check
    cmp x10, #0x16
    b.ne fail
    and x10, x9, #0xffff
    cmp x10, #0x42
    b.ne fail
    adr x10, phase
    ldr x11, [x10]
    cmp x11, #2
    b.ne fail
    adr x10, guest_data
    ldr x11, [x10]
    cmp x11, #0x123
    b.ne fail
    adr x10, secret
    ldr x11, [x10]
    mov x12, #0x5a5a
    cmp x11, x12
    b.ne fail
    adr x1, passed
    b report
abort_check:
    /* Only expected L3 translation faults, not arbitrary exceptions. */
    and x10, x9, #0x3f
    cmp x10, #7
    b.ne fail
    tbnz x9, #10, fail
    tbnz x9, #7, fail
    mrs x10, far_el2
    ldr x11, =0x40080000
    cmp x10, x11
    b.ne fail
    mrs x10, hpfar_el2
    lsr x11, x11, #8
    cmp x10, x11
    b.ne fail
    adr x10, phase
    ldr x11, [x10]
    cmp x11, #0
    b.eq read_fault
    cmp x11, #1
    b.ne fail
    tbz x9, #6, fail
    b advance
read_fault:
    tbnz x9, #6, fail
advance:
    add x11, x11, #1
    str x11, [x10]
    mrs x10, elr_el2
    add x10, x10, #4
    msr elr_el2, x10
    eret
blocked:
    adr x1, blocked_msg
    b report
fail:
    adr x1, failed
report:
    mov x2, #0x09000000
2:  ldrb w3, [x1], #1
    cbz w3, 4f
3:  ldr w4, [x2, #0x18]
    tbnz w4, #5, 3b
    str w3, [x2]
    b 2b
4:  wfe
    b 4b
    .ltorg

/* Separate physical page, mapped at a different guest IPA. */
.section .text.guest,"ax"
.balign 4096
guest:
    mov x0, #0x40000000
    add x0, x0, #2, lsl #12
    mov x1, #0x123
    str x1, [x0]
    ldr x2, [x0]
    cmp x1, x2
    b.ne guest_bad
    mov x3, #0x40000000
    add x3, x3, #0x80, lsl #12
    ldr x4, [x3]
    str x1, [x3]
    hvc #0x42
guest_bad:
    hvc #0x43
    b guest_bad
.balign 4096

.section .rodata,"a"
passed: .asciz "EXODUS_S2_REF_PASS\n"
blocked_msg: .asciz "EXODUS_S2_REF_EL1_BLOCKED\n"
failed: .asciz "EXODUS_S2_REF_FAIL\n"
.section .bss,"aw",%nobits
.balign 4096
l1: .skip 4096
l2: .skip 4096
l3: .skip 4096
guest_data: .skip 4096
phase: .skip 8
secret: .skip 8
.balign 16
stack: .skip 4096
stack_top:

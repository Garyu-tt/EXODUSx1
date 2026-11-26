/* QEMU virt only, 4-core PSCI bootstrap and per-core stage-2 fixture. */
.section .text.boot,"ax"
.global _start
_start:
    mrs x0, CurrentEL
    cmp x0, #8
    b.ne blocked
    msr daifset, #15
    msr spsel, #1
    adr x0, zero_start
    adr x1, zero_end
1:  str xzr, [x0], #8
    cmp x0, x1
    b.lo 1b
    dsb sy
    mov x20, #1
2:  ldr x0, =0xc4000003 /* PSCI CPU_ON */
    mov x1, x20
    adr x2, secondary
    mov x3, x20
    smc #0
    cbnz x0, fail
    add x20, x20, #1
    cmp x20, #4
    b.lo 2b
    b core_init
secondary:
    msr daifset, #15
    mrs x1, CurrentEL
    cmp x1, #8
    b.ne secondary_fail
    msr spsel, #1
    mrs x1, mpidr_el1
    and x1, x1, #0xff
    cmp x0, x1
    b.ne secondary_fail
core_init:
    mrs x19, mpidr_el1
    and x19, x19, #0xff
    cmp x19, #4
    b.hs secondary_fail
    adr x0, stacks
    add x1, x19, #1
    add x0, x0, x1, lsl #12
    mov sp, x0
    adr x0, vectors
    msr vbar_el2, x0
    adr x0, roots
    mov x1, #12288
    madd x0, x19, x1, x0
    add x2, x0, #1, lsl #12
    add x3, x0, #2, lsl #12
    orr x4, x2, #3
    str x4, [x0, #8]
    orr x4, x3, #3
    str x4, [x2]
    adr x4, guest
    mov x5, #0x77f
    orr x4, x4, x5
    str x4, [x3, #8]
    adr x4, data
    add x4, x4, x19, lsl #12
    mov x5, #0x7ff
    orr x4, x4, x5
    orr x4, x4, #(1 << 54)
    str x4, [x3, #16]
    dsb sy
    ldr x1, =0x80003559
    msr vtcr_el2, x1
    add x1, x19, #1
    orr x0, x0, x1, lsl #48
    msr vttbr_el2, x0
    isb
    tlbi vmalls12e1
    dsb sy
    isb
    ldr x0, =0x80000001
    msr hcr_el2, x0
    ldr x0, =0x30d00800
    msr sctlr_el1, x0
    isb
    /* All four must reach EL2 setup before any guest executes. */
    adr x0, ready
    mov x1, #1
    add x0, x0, x19, lsl #3
    stlr x1, [x0]
    adr x0, ready
3:  ldar x1, [x0]
    cbz x1, 3b
    add x0, x0, #8
    adr x2, done
    cmp x0, x2
    b.lo 3b
    ldr x0, =0x40001000
    msr elr_el2, x0
    mov x0, #0x3c5
    msr spsr_el2, x0
    mov x20, #0
    eret
    .ltorg

.balign 2048
vectors:
.rept 8
    b core_fail
    .balign 128
.endr
    b lower_sync
    .balign 128
.rept 7
    b core_fail
    .balign 128
.endr

lower_sync:
    mrs x9, mpidr_el1
    and x9, x9, #0xff
    cmp x9, x19
    b.ne core_fail
    mrs x10, spsr_el2
    and x10, x10, #15
    cmp x10, #5
    b.ne core_fail
    mrs x10, esr_el2
    ldr x11, =0x5a000042
    cmp x10, x11
    b.ne core_fail
    adr x10, counts
    add x10, x10, x9, lsl #3
    ldr x11, [x10]
    add x11, x11, #1
    cmp x11, x20
    b.ne core_fail
    str x11, [x10]
    cmp x11, #64
    b.lo resume
    b.hi core_fail
    adr x10, data
    add x10, x10, x9, lsl #12
    ldp x11, x12, [x10]
    cmp x11, #64
    b.ne core_fail
    add x13, x9, #1
    cmp x12, x13
    b.ne core_fail
    adr x10, done
    add x10, x10, x9, lsl #3
    mov x11, #1
    stlr x11, [x10]
    cbnz x9, halt
    /* Primary reports only after acquiring all completion flags. */
    adr x10, done
4:  ldar x11, [x10]
    cbz x11, 4b
    cmp x11, #1
    b.ne fail
    add x10, x10, #8
    adr x12, counts
    cmp x10, x12
    b.lo 4b
    adr x1, passed
    b report
resume:
    eret
core_fail:
    mrs x9, mpidr_el1
    and x9, x9, #0xff
    cbz x9, fail
    b mark_fail
secondary_fail:
    mrs x9, mpidr_el1
    and x9, x9, #0xff
mark_fail:
    cmp x9, #4
    b.hs halt
    adr x10, done
    add x10, x10, x9, lsl #3
    mov x11, #2
    stlr x11, [x10]
    adr x10, ready
    add x10, x10, x9, lsl #3
    stlr x11, [x10]
    b halt
blocked:
    adr x1, blocked_msg
    b report
fail:
    adr x1, failed
report:
    mov x2, #0x09000000
5:  ldrb w3, [x1], #1
    cbz w3, halt
6:  ldr w4, [x2, #0x18]
    tbnz w4, #5, 6b
    str w3, [x2]
    b 5b
halt:
    wfe
    b halt
    .ltorg

.section .text.guest,"ax"
.balign 4096
guest:
    mov x0, #0x40000000
    add x0, x0, #2, lsl #12
    add x1, x19, #1
    str x1, [x0, #8]
7:  ldr x2, [x0]
    cmp x2, x20
    b.ne guest_bad
    ldr x2, [x0, #8]
    cmp x2, x1
    b.ne guest_bad
    add x20, x20, #1
    str x20, [x0]
    hvc #0x42
    b 7b
guest_bad:
    hvc #0x43
    b guest_bad
.balign 4096
.section .rodata,"a"
passed: .asciz "EXODUS_SMP_REF_PASS\n"
blocked_msg: .asciz "EXODUS_SMP_REF_EL1_BLOCKED\n"
failed: .asciz "EXODUS_SMP_REF_FAIL\n"
.section .bss,"aw",%nobits
.balign 4096
zero_start:
roots: .skip 49152
data: .skip 16384
ready: .skip 32
done: .skip 32
counts: .skip 32
zero_end:
.balign 4096
stacks: .skip 16384

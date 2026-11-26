/* QEMU virt only. Timer-preempted trusted EL1 fixtures, not a production scheduler. */
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
    adr x0, zero_start
    adr x1, zero_end
1:  str xzr, [x0], #8
    cmp x0, x1
    b.lo 1b
    adr x0, tables_a
    adr x1, data_a
    bl map
    adr x0, tables_b
    adr x1, data_b
    bl map
    adr x0, ctx_a
    mov x1, #1
    bl init_ctx
    adr x0, ctx_b
    mov x1, #2
    bl init_ctx
    dsb sy
    ldr x0, =0x80003559
    msr vtcr_el2, x0
    ldr x0, =0x80000011
    msr hcr_el2, x0
    ldr x0, =0x30d00800
    msr sctlr_el1, x0
    isb
    bl timer_setup
    b enter

/* QEMU virt GICv2 and non-secure EL2 physical timer PPI 26.
 * No interrupt controller address is portable to the physical target. */
timer_setup:
    msr cnthp_ctl_el2, xzr
    mov x0, #0x08000000
    str wzr, [x0]
    mov w1, #(1 << 26)
    str w1, [x0, #0x180] /* disable PPI */
    str w1, [x0, #0x280] /* clear pending */
    mov w2, #0x80
    strb w2, [x0, #0x41a] /* priority */
    str w1, [x0, #0x100] /* enable PPI */
    mov w1, #1
    str w1, [x0]
    mov x0, #0x08000000
    add x0, x0, #0x10, lsl #12
    mov w1, #0xff
    str w1, [x0, #4]
    mov w1, #1
    str w1, [x0]
    dsb sy
    mrs x0, cntfrq_el0
    mov x1, #100
    udiv x0, x0, x1 /* 10 ms nominal */
    cbz x0, fail
    lsr x1, x0, #31
    cbnz x1, fail
    adr x1, quantum
    str x0, [x1]
    ret

/* x0: root of three contiguous tables; x1: private data page. */
map:
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
    mov x5, #0x7ff
    orr x4, x1, x5
    orr x4, x4, #(1 << 54)
    str x4, [x3, #16]
    ret

/* x0: context, x1: distinct guest identity. */
init_ctx:
    str x1, [x0, #152] /* x19 */
    ldr x2, =0x40001000
    str x2, [x0, #248] /* ELR */
    mov x2, #0x345
    str x2, [x0, #256] /* SPSR */
    ldr x2, =0x40003000
    str x2, [x0, #264] /* SP_EL0 */
    str x2, [x0, #272] /* SP_EL1 */
    ret
    .ltorg

.balign 2048
vectors:
.rept 8
    b fail
    .balign 128
.endr
    b fail
    .balign 128
    b lower_irq
    .balign 128
.rept 6
    b fail
    .balign 128
.endr

lower_irq:
    stp x0, x1, [sp, #-16]!
    adr x0, active
    ldr x0, [x0]
    cbnz x0, 2f
    adr x0, ctx_a
    b 3f
2:  adr x0, ctx_b
3:  stp x2, x3, [x0, #16]
    stp x4, x5, [x0, #32]
    stp x6, x7, [x0, #48]
    stp x8, x9, [x0, #64]
    stp x10, x11, [x0, #80]
    stp x12, x13, [x0, #96]
    stp x14, x15, [x0, #112]
    stp x16, x17, [x0, #128]
    stp x18, x19, [x0, #144]
    stp x20, x21, [x0, #160]
    stp x22, x23, [x0, #176]
    stp x24, x25, [x0, #192]
    stp x26, x27, [x0, #208]
    stp x28, x29, [x0, #224]
    str x30, [x0, #240]
    ldp x2, x3, [sp], #16
    stp x2, x3, [x0]
    mrs x2, elr_el2
    str x2, [x0, #248]
    mrs x2, spsr_el2
    str x2, [x0, #256]
    and x2, x2, #15
    cmp x2, #5
    b.ne fail
    mrs x2, sp_el0
    str x2, [x0, #264]
    mrs x2, sp_el1
    str x2, [x0, #272]
    /* Acknowledge exact timer source, verify asserted timer, disable, EOI. */
    mov x2, #0x08000000
    add x2, x2, #0x10, lsl #12
    ldr w3, [x2, #0x0c]
    and w4, w3, #0x3ff
    cmp w4, #26
    b.ne fail
    mrs x4, cnthp_ctl_el2
    and x4, x4, #7
    cmp x4, #5
    b.ne fail
    msr cnthp_ctl_el2, xzr
    isb
    str w3, [x2, #0x10]
    dsb sy
    adr x4, active
    ldr x5, [x4]
    add x6, x5, #1
    ldr x7, [x0, #152]
    cmp x6, x7
    b.ne fail
    /* Per-guest progress is measured from saved x20, not synthesized. */
    ldr x7, [x0, #160]
    adr x8, samples
    ldr x9, [x8, x5, lsl #3]
    cmp x7, x9
    b.lo fail
    str x7, [x8, x5, lsl #3]
    adr x8, yields
    ldr x9, [x8]
    add x9, x9, #1
    str x9, [x8]
    cmp x9, #32
    b.eq finish
    eor x5, x5, #1
    str x5, [x4]

enter:
    adr x0, active
    ldr x0, [x0]
    cbnz x0, 4f
    adr x1, tables_a
    adr x30, ctx_a
    mov x2, #1
    b 5f
4:  adr x1, tables_b
    adr x30, ctx_b
    mov x2, #2
5:  orr x1, x1, x2, lsl #48
    dsb ish
    msr vttbr_el2, x1
    isb
    tlbi vmalls12e1is
    dsb ish
    isb
    ldr x0, [x30, #248]
    msr elr_el2, x0
    ldr x0, [x30, #256]
    msr spsr_el2, x0
    ldr x0, [x30, #264]
    msr sp_el0, x0
    ldr x0, [x30, #272]
    msr sp_el1, x0
    adr x0, quantum
    ldr x0, [x0]
    msr cnthp_tval_el2, x0
    mov x0, #1
    msr cnthp_ctl_el2, x0
    isb
    ldp x0, x1, [x30]
    ldp x2, x3, [x30, #16]
    ldp x4, x5, [x30, #32]
    ldp x6, x7, [x30, #48]
    ldp x8, x9, [x30, #64]
    ldp x10, x11, [x30, #80]
    ldp x12, x13, [x30, #96]
    ldp x14, x15, [x30, #112]
    ldp x16, x17, [x30, #128]
    ldp x18, x19, [x30, #144]
    ldp x20, x21, [x30, #160]
    ldp x22, x23, [x30, #176]
    ldp x24, x25, [x30, #192]
    ldp x26, x27, [x30, #208]
    ldp x28, x29, [x30, #224]
    ldr x30, [x30, #240]
    eret

finish:
    /* Both saved counters must be nonzero; stores can lag x20 by one
     * when interrupted between ADD and STR. Private identity tags differ. */
    adr x0, samples
    ldp x1, x2, [x0]
    cbz x1, fail
    cbz x2, fail
    adr x0, data_a
    ldr x3, [x0]
    sub x3, x1, x3
    cmp x3, #1
    b.hi fail
    ldr x3, [x0, #8]
    cmp x3, #1
    b.ne fail
    adr x0, data_b
    ldr x3, [x0]
    sub x3, x2, x3
    cmp x3, #1
    b.hi fail
    ldr x3, [x0, #8]
    cmp x3, #2
    b.ne fail
    adr x1, passed
    b report

blocked:
    adr x1, blocked_msg
    b report
fail:
    adr x1, failed
report:
    mov x2, #0x09000000
6:  ldrb w3, [x1], #1
    cbz w3, 8f
7:  ldr w4, [x2, #0x18]
    tbnz w4, #5, 7b
    str w3, [x2]
    b 6b
8:  wfe
    b 8b
    .ltorg

.section .text.guest,"ax"
.balign 4096
guest:
    mov x0, #0x40000000
    add x0, x0, #2, lsl #12
    str x19, [x0, #8]
    add x21, x19, #0x100
    mov x30, x21
    mov x18, x21
9:  ldr x1, [x0]
    cmp x1, x20
    b.ne guest_bad
    ldr x1, [x0, #8]
    cmp x1, x19
    b.ne guest_bad
    add x20, x20, #1
    str x20, [x0]
    cmp x18, x21
    b.ne guest_bad
    cmp x30, x21
    b.ne guest_bad
    b 9b
guest_bad:
    hvc #0x43
    b guest_bad
.balign 4096
.section .rodata,"a"
passed: .asciz "EXODUS_TIMER_REF_PASS\n"
blocked_msg: .asciz "EXODUS_TIMER_REF_EL1_BLOCKED\n"
failed: .asciz "EXODUS_TIMER_REF_FAIL\n"
.section .bss,"aw",%nobits
.balign 4096
zero_start:
tables_a: .skip 12288
tables_b: .skip 12288
data_a: .skip 4096
data_b: .skip 4096
ctx_a: .skip 288
ctx_b: .skip 288
active: .skip 8
yields: .skip 8
samples: .skip 16
quantum: .skip 8
zero_end:
.balign 16
stack: .skip 4096
stack_top:

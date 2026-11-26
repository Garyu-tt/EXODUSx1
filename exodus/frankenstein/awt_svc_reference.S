/* R09: QEMU virt only, 4 cores, 8 trusted EL1 contexts, timer preemption. */
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
    adr x0, stacks
    add x0, x0, #1, lsl #12
    mov sp, x0
    adr x0, elf_backing
    bl elf_boot
    cbz x0, fail
    /* Primary builds all eight contexts before releasing secondaries. */
    mov x20, #0
12: adr x0, roots
    mov x3, #12288
    madd x0, x20, x3, x0
    adr x1, data
    add x1, x1, x20, lsl #12
    bl map
    adr x0, roots
    mov x3, #12288
    madd x0, x20, x3, x0
    adr x1, s1_tables
    madd x1, x20, x3, x1
    add x2, x20, #1
    bl map_s1
    adr x0, contexts
    mov x3, #960
    madd x0, x20, x3, x0
    add x1, x20, #1
    bl init_ctx
    add x20, x20, #1
    cmp x20, #8
    b.lo 12b
    mrs x0, cntfrq_el0
    mov x1, #100
    udiv x0, x0, x1
    cbz x0, fail
    lsr x1, x0, #31
    cbnz x1, fail
    adr x1, quantum
    str x0, [x1]
    mov x0, #0x08000000
    mov w1, #1
    str w1, [x0] /* global GIC distributor enabled once */
    dsb sy
    mov x20, #1
13: ldr x0, =0xc4000003
    mov x1, x20
    adr x2, secondary
    mov x3, x20
    smc #0
    cbnz x0, fail
    add x20, x20, #1
    cmp x20, #4
    b.lo 13b
    b core_init
secondary:
    msr daifset, #15
    mrs x1, CurrentEL
    cmp x1, #8
    b.ne fail
    msr spsel, #1
    mrs x1, mpidr_el1
    and x1, x1, #0xff
    cmp x0, x1
    b.ne fail
core_init:
    mrs x19, mpidr_el1
    and x19, x19, #0xff
    cmp x19, #4
    b.hs fail
    adr x0, stacks
    add x1, x19, #1
    add x0, x0, x1, lsl #12
    mov sp, x0
    adr x0, vectors
    msr vbar_el2, x0
    ldr x0, =0x80003559
    msr vtcr_el2, x0
    ldr x0, =0x80000011
    msr hcr_el2, x0
    ldr x0, =0x30d00800
    msr sctlr_el1, x0
    mov x0, #0x33ff
    msr cptr_el2, x0
    mov x0, #0x300000
    msr cpacr_el1, x0
    isb
    /* Banked GICv2 PPI and CPU-interface configuration per core. */
    msr cnthp_ctl_el2, xzr
    mov x0, #0x08000000
    mov w1, #(1 << 26)
    str w1, [x0, #0x180]
    str w1, [x0, #0x280]
    mov w2, #0x80
    strb w2, [x0, #0x41a]
    str w1, [x0, #0x100]
    add x0, x0, #0x10, lsl #12
    mov w1, #0xff
    str w1, [x0, #4]
    mov w1, #1
    str w1, [x0]
    dsb sy
    cbnz x19, amp_boot_done
    bl amp_boot
    cbz x0, fail
amp_boot_done:
    adr x0, ready
    add x0, x0, x19, lsl #3
    mov x1, #1
    stlr x1, [x0]
    adr x0, ready
14: ldar x1, [x0]
    cbz x1, 14b
    cmp x1, #1
    b.ne fail
    add x0, x0, #8
    adr x1, done
    cmp x0, x1
    b.lo 14b
    b enter
    .ltorg

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
    adr x4, elf_backing
    mov x6, #12288
    madd x4, x20, x6, x4
    mov x5, #0x77f
    orr x6, x4, x5
    str x6, [x3, #320] /* ELF RX IPA 0x40028000 */
    add x6, x4, #2, lsl #12
    mov x7, #0x7ff
    orr x6, x6, x7
    orr x6, x6, #(1 << 54)
    str x6, [x3, #328] /* ELF RW IPA 0x40029000 */
    adr x4, service_vectors
    orr x4, x4, x5
    str x4, [x3, #288] /* EL1 RX IPA 0x40024000 */
    ret

/* Stage-1 table pages are read-only to guests, mapped via stage-2.
 * A and B have different root IPAs, ASIDs and physical table pages.
 * Both translate VA 0x1000/0x2000 to their own code/data IPAs. */
map_s1:
    add x3, x0, #2, lsl #12
    /* Root IPA 0x40004000 (A) or 0x40008000 (B). */
    lsl x4, x2, #2
    mov x5, #0x77f
    orr x5, x5, #(1 << 54)
    mov x6, #0x40000000
    add x6, x6, x4, lsl #12
    mov x7, #3
11: orr x8, x1, x5
    str x8, [x3, x4, lsl #3]
    add x1, x1, #1, lsl #12
    add x4, x4, #1
    subs x7, x7, #1
    b.ne 11b
    sub x1, x1, #3, lsl #12
    add x7, x6, #1, lsl #12
    orr x7, x7, #3
    str x7, [x1]
    add x7, x6, #2, lsl #12
    orr x7, x7, #3
    str x7, [x1, #4096]
    add x1, x1, #2, lsl #12
    ldr x7, =0x0040000040001783
    str x7, [x1, #8]
    ldr x7, =0x0060000040002703
    str x7, [x1, #16]
    ldr x7, =0x00200000400287c3 /* ELF EL0 RX, PXN */
    str x7, [x1, #24]
    ldr x7, =0x0040000040024783 /* EL1 RX, UXN */
    str x7, [x1, #32]
    ldr x7, =0x0060000040029743 /* ELF EL0 RW, UXN/PXN */
    str x7, [x1, #40]
    ret
    .ltorg

/* x0: context, x1: distinct guest identity. */
init_ctx:
    str x1, [x0, #152] /* x19 */
    mov x2, #0x1000
    str x2, [x0, #248] /* ELR */
    mov x2, #0x345
    str x2, [x0, #256] /* SPSR */
    mov x2, #0x3000
    str x2, [x0, #264] /* SP_EL0 */
    str x2, [x0, #272] /* SP_EL1 */
    lsl x2, x1, #14
    mov x3, #0x40000000
    add x2, x2, x3
    orr x2, x2, x1, lsl #48
    str x2, [x0, #864] /* TTBR0 */
    ldr x2, =0x80993519 /* 39-bit VA, 4KiB, WBWA, EPD1 */
    str x2, [x0, #880]
    mov x2, #0xff
    str x2, [x0, #888] /* MAIR */
    ldr x2, =0x30d00801 /* stage-1 M=1 */
    str x2, [x0, #912]
    mov x2, #0x300000
    str x2, [x0, #920]
    ret
    .ltorg

.balign 2048
vectors:
.rept 8
    b fail
    .balign 128
.endr
    b lower_sync
    .balign 128
    b lower_irq
    .balign 128
.rept 6
    b fail
    .balign 128
.endr

lower_sync:
    stp x0, x1, [sp, #-16]!
    mov x0, #1
    b save_common
lower_irq:
    stp x0, x1, [sp, #-16]!
    mov x0, #0
save_common:
    str x0, [sp, #-16]!
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    adr x1, active
    ldr x1, [x1, x0, lsl #3]
    add x0, x1, x0, lsl #1
    mov x1, #960
    mul x0, x0, x1
    adr x1, contexts
    add x0, x0, x1
    stp x2, x3, [x0, #16]
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
    ldr x2, [sp], #16
    str x2, [x0, #848]
    ldp x2, x3, [sp], #16
    stp x2, x3, [x0]
    mrs x2, elr_el2
    str x2, [x0, #248]
    mrs x2, spsr_el2
    str x2, [x0, #256]
    and x2, x2, #15
    cmp x2, #5
    b.eq saved_mode_ok
    cbnz x2, fail
saved_mode_ok:
    mrs x2, sp_el0
    str x2, [x0, #264]
    mrs x2, sp_el1
    str x2, [x0, #272]
    stp q0, q1, [x0, #288]
    stp q2, q3, [x0, #320]
    stp q4, q5, [x0, #352]
    stp q6, q7, [x0, #384]
    stp q8, q9, [x0, #416]
    stp q10, q11, [x0, #448]
    stp q12, q13, [x0, #480]
    stp q14, q15, [x0, #512]
    stp q16, q17, [x0, #544]
    stp q18, q19, [x0, #576]
    stp q20, q21, [x0, #608]
    stp q22, q23, [x0, #640]
    stp q24, q25, [x0, #672]
    stp q26, q27, [x0, #704]
    stp q28, q29, [x0, #736]
    stp q30, q31, [x0, #768]
    mrs x2, fpcr
    str x2, [x0, #800]
    mrs x2, fpsr
    str x2, [x0, #808]
    mrs x2, tpidr_el0
    str x2, [x0, #816]
    mrs x2, tpidrro_el0
    str x2, [x0, #824]
    mrs x2, tpidr_el1
    str x2, [x0, #832]
    mrs x2, contextidr_el1
    str x2, [x0, #840]
    mrs x2, ttbr0_el1
    str x2, [x0, #864]
    mrs x2, ttbr1_el1
    str x2, [x0, #872]
    mrs x2, tcr_el1
    str x2, [x0, #880]
    mrs x2, mair_el1
    str x2, [x0, #888]
    mrs x2, amair_el1
    str x2, [x0, #896]
    mrs x2, vbar_el1
    str x2, [x0, #904]
    mrs x2, sctlr_el1
    str x2, [x0, #912]
    mrs x2, cpacr_el1
    str x2, [x0, #920]
    mrs x2, elr_el1
    str x2, [x0, #928]
    mrs x2, spsr_el1
    str x2, [x0, #936]
    mrs x2, esr_el1
    str x2, [x0, #944]
    mrs x2, far_el1
    str x2, [x0, #952]
    ldr x2, [x0, #848]
    cbnz x2, sync_saved
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
    mrs x12, mpidr_el1
    and x12, x12, #0xff
    adr x4, active
    add x4, x4, x12, lsl #3
    ldr x5, [x4]
    add x6, x5, x12, lsl #1
    add x7, x6, #1
    ldr x8, [x0, #152]
    cmp x7, x8
    b.ne fail
    ldr x7, [x0, #160]
    adr x8, samples
    add x8, x8, x6, lsl #3
    ldr x9, [x8]
    cmp x7, x9
    b.lo fail
    str x7, [x8]
    adr x8, yields
    add x8, x8, x12, lsl #3
    ldr x9, [x8]
    add x9, x9, #1
    str x9, [x8]
    cmp x9, #128
    b.eq finish_core
    eor x5, x5, #1
    str x5, [x4]
    b enter

sync_saved:
    mrs x2, esr_el2
    ldr x3, =0x5a000044
    cmp x2, x3
    b.ne fail
    /* Only the trusted EL1 SVC relay may submit. */
    ldr x2, [x0, #256]
    and x2, x2, #15
    cmp x2, #5
    b.ne fail
    ldr x2, [x0, #936]
    tst x2, #15
    b.ne fail
    ldr x2, [x0, #944]
    ldr x3, =0x56000000
    cmp x2, x3
    b.ne fail
    ldr x2, [x0, #248]
    adr x3, relay_after_hvc
    adr x4, service_vectors
    sub x3, x3, x4
    add x3, x3, #4, lsl #12
    cmp x2, x3
    b.ne fail
    msr cnthp_ctl_el2, xzr
    isb
    mov x19, x0
    mrs x1, mpidr_el1
    and x1, x1, #0xff
    adr x2, active
    ldr x2, [x2, x1, lsl #3]
    add x0, x2, x1, lsl #1
    ldr x2, [x19, #64] /* FreeBSD syscall number x8 */
    ldr x3, [x19]      /* first argument x0 */
    mov x4, x19
    bl amp_submit
    cbz x0, fail
    /* Prefer the sibling; blocked contexts never ERET until reply. */
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    adr x1, active
    add x1, x1, x0, lsl #3
    ldr x2, [x1]
    eor x2, x2, #1
    str x2, [x1]

enter:
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    adr x1, active
    ldr x1, [x1, x0, lsl #3]
    adr x2, contexts
    adr x3, elf_backing
    bl amp_pick
    cmp x0, #2
    b.eq enter /* both blocked: local bounded idle pump, not guest spin */
    cmp x0, #1
    b.hi fail
    mov x1, x0
    mrs x0, mpidr_el1
    and x0, x0, #0xff
    adr x2, active
    str x1, [x2, x0, lsl #3]
    add x0, x1, x0, lsl #1
    mov x2, #960
    adr x30, contexts
    madd x30, x0, x2, x30
    mov x2, #12288
    adr x1, roots
    madd x1, x0, x2, x1
    add x2, x0, #1
    orr x1, x1, x2, lsl #48
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
    ldp q0, q1, [x30, #288]
    ldp q2, q3, [x30, #320]
    ldp q4, q5, [x30, #352]
    ldp q6, q7, [x30, #384]
    ldp q8, q9, [x30, #416]
    ldp q10, q11, [x30, #448]
    ldp q12, q13, [x30, #480]
    ldp q14, q15, [x30, #512]
    ldp q16, q17, [x30, #544]
    ldp q18, q19, [x30, #576]
    ldp q20, q21, [x30, #608]
    ldp q22, q23, [x30, #640]
    ldp q24, q25, [x30, #672]
    ldp q26, q27, [x30, #704]
    ldp q28, q29, [x30, #736]
    ldp q30, q31, [x30, #768]
    ldr x0, [x30, #800]
    msr fpcr, x0
    ldr x0, [x30, #808]
    msr fpsr, x0
    ldr x0, [x30, #816]
    msr tpidr_el0, x0
    ldr x0, [x30, #824]
    msr tpidrro_el0, x0
    ldr x0, [x30, #832]
    msr tpidr_el1, x0
    ldr x0, [x30, #840]
    msr contextidr_el1, x0
    isb
    ldr x0, [x30, #864]
    msr ttbr0_el1, x0
    ldr x0, [x30, #872]
    msr ttbr1_el1, x0
    ldr x0, [x30, #880]
    msr tcr_el1, x0
    ldr x0, [x30, #888]
    msr mair_el1, x0
    ldr x0, [x30, #896]
    msr amair_el1, x0
    ldr x0, [x30, #904]
    msr vbar_el1, x0
    ldr x0, [x30, #912]
    msr sctlr_el1, x0
    ldr x0, [x30, #920]
    msr cpacr_el1, x0
    ldr x0, [x30, #928]
    msr elr_el1, x0
    ldr x0, [x30, #936]
    msr spsr_el1, x0
    ldr x0, [x30, #944]
    msr esr_el1, x0
    ldr x0, [x30, #952]
    msr far_el1, x0
    isb
    tlbi vmalls12e1is
    dsb ish
    isb
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

finish_core:
    adr x0, done
    add x0, x0, x12, lsl #3
    mov x1, #1
    stlr x1, [x0]
    cbnz x12, halt
    adr x19, done
15: ldar x1, [x19]
    cbnz x1, finish_flag
    bl amp_finish_pump
    cbz x0, fail
    b 15b
finish_flag:
    cmp x1, #1
    b.ne fail
    add x19, x19, #8
    adr x1, active
    cmp x19, x1
    b.lo 15b
    bl amp_all_done
    cbz x0, fail
    adr x0, elf_backing
    bl elf_verify
    cbz x0, fail
    /* All cores stopped; independently verify all eight saved contexts. */
    mov x20, #0
16: adr x0, samples
    ldr x1, [x0, x20, lsl #3]
    cbz x1, fail
    adr x0, data
    add x0, x0, x20, lsl #12
    ldp x2, x3, [x0]
    ldr x4, [x0, #16]
    cmp x4, #1
    b.ne fail
    sub x2, x1, x2
    cmp x2, #1
    b.hi fail
    add x21, x20, #1
    cmp x3, x21
    b.ne fail
    adr x0, contexts
    mov x3, #960
    madd x0, x20, x3, x0
    ldr x2, [x0, #864]
    lsl x3, x21, #14
    mov x4, #0x40000000
    add x3, x3, x4
    orr x3, x3, x21, lsl #48
    cmp x2, x3
    b.ne fail
    add x1, x21, #0x100
    add x0, x0, #288
    mov x2, #32
17: ldp x3, x4, [x0], #16
    cmp x3, x1
    b.ne fail
    add x5, x1, #0x80
    cmp x4, x5
    b.ne fail
    add x1, x1, #1
    subs x2, x2, #1
    b.ne 17b
    add x20, x20, #1
    cmp x20, #8
    b.lo 16b
    adr x1, passed
    b report

blocked:
    adr x1, blocked_msg
    b report
fail:
    mrs x9, mpidr_el1
    and x9, x9, #0xff
    cbz x9, primary_fail
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
primary_fail:
    adr x1, failed
report:
    mov x2, #0x09000000
6:  ldrb w3, [x1], #1
    cbz w3, 8f
7:  ldr w4, [x2, #0x18]
    tbnz w4, #5, 7b
    str w3, [x2]
    b 6b
halt:
8:  wfe
    b 8b
    .ltorg

.section .text.guest,"ax"
.balign 4096
guest:
    mov x0, #0x2000
    str x19, [x0, #8]
    add x21, x19, #0x100
    mov x30, x21
    mov x18, x21
    add x2, x21, #0
    dup v0.2d, x2
    add x2, x2, #0x80
    ins v0.d[1], x2
    add x2, x21, #1
    dup v1.2d, x2
    add x2, x2, #0x80
    ins v1.d[1], x2
    add x2, x21, #2
    dup v2.2d, x2
    add x2, x2, #0x80
    ins v2.d[1], x2
    add x2, x21, #3
    dup v3.2d, x2
    add x2, x2, #0x80
    ins v3.d[1], x2
    add x2, x21, #4
    dup v4.2d, x2
    add x2, x2, #0x80
    ins v4.d[1], x2
    add x2, x21, #5
    dup v5.2d, x2
    add x2, x2, #0x80
    ins v5.d[1], x2
    add x2, x21, #6
    dup v6.2d, x2
    add x2, x2, #0x80
    ins v6.d[1], x2
    add x2, x21, #7
    dup v7.2d, x2
    add x2, x2, #0x80
    ins v7.d[1], x2
    add x2, x21, #8
    dup v8.2d, x2
    add x2, x2, #0x80
    ins v8.d[1], x2
    add x2, x21, #9
    dup v9.2d, x2
    add x2, x2, #0x80
    ins v9.d[1], x2
    add x2, x21, #10
    dup v10.2d, x2
    add x2, x2, #0x80
    ins v10.d[1], x2
    add x2, x21, #11
    dup v11.2d, x2
    add x2, x2, #0x80
    ins v11.d[1], x2
    add x2, x21, #12
    dup v12.2d, x2
    add x2, x2, #0x80
    ins v12.d[1], x2
    add x2, x21, #13
    dup v13.2d, x2
    add x2, x2, #0x80
    ins v13.d[1], x2
    add x2, x21, #14
    dup v14.2d, x2
    add x2, x2, #0x80
    ins v14.d[1], x2
    add x2, x21, #15
    dup v15.2d, x2
    add x2, x2, #0x80
    ins v15.d[1], x2
    add x2, x21, #16
    dup v16.2d, x2
    add x2, x2, #0x80
    ins v16.d[1], x2
    add x2, x21, #17
    dup v17.2d, x2
    add x2, x2, #0x80
    ins v17.d[1], x2
    add x2, x21, #18
    dup v18.2d, x2
    add x2, x2, #0x80
    ins v18.d[1], x2
    add x2, x21, #19
    dup v19.2d, x2
    add x2, x2, #0x80
    ins v19.d[1], x2
    add x2, x21, #20
    dup v20.2d, x2
    add x2, x2, #0x80
    ins v20.d[1], x2
    add x2, x21, #21
    dup v21.2d, x2
    add x2, x2, #0x80
    ins v21.d[1], x2
    add x2, x21, #22
    dup v22.2d, x2
    add x2, x2, #0x80
    ins v22.d[1], x2
    add x2, x21, #23
    dup v23.2d, x2
    add x2, x2, #0x80
    ins v23.d[1], x2
    add x2, x21, #24
    dup v24.2d, x2
    add x2, x2, #0x80
    ins v24.d[1], x2
    add x2, x21, #25
    dup v25.2d, x2
    add x2, x2, #0x80
    ins v25.d[1], x2
    add x2, x21, #26
    dup v26.2d, x2
    add x2, x2, #0x80
    ins v26.d[1], x2
    add x2, x21, #27
    dup v27.2d, x2
    add x2, x2, #0x80
    ins v27.d[1], x2
    add x2, x21, #28
    dup v28.2d, x2
    add x2, x2, #0x80
    ins v28.d[1], x2
    add x2, x21, #29
    dup v29.2d, x2
    add x2, x2, #0x80
    ins v29.d[1], x2
    add x2, x21, #30
    dup v30.2d, x2
    add x2, x2, #0x80
    ins v30.d[1], x2
    add x2, x21, #31
    dup v31.2d, x2
    add x2, x2, #0x80
    ins v31.d[1], x2
    and x2, x19, #3
    lsl x2, x2, #22
    msr fpcr, x2
    lsl x2, x19, #0
    msr fpsr, x2
    add x2, x21, #0x200
    msr tpidr_el0, x2
    add x2, x21, #0x300
    msr tpidrro_el0, x2
    add x2, x21, #0x400
    msr tpidr_el1, x2
    add x2, x21, #0x500
    msr contextidr_el1, x2
    mov x2, #0x4000
    msr vbar_el1, x2
    add x2, x21, #0x600
    msr elr_el1, x2
    add x2, x21, #0x700
    msr far_el1, x2
    isb
9:
    mrs x2, ttbr0_el1
    lsl x3, x19, #14
    mov x4, #0x40000000
    add x3, x3, x4
    orr x3, x3, x19, lsl #48
    cmp x2, x3
    b.ne guest_bad
    mrs x2, sctlr_el1
    tbz x2, #0, guest_bad
    mrs x2, vbar_el1
    cmp x2, #0x4000
    b.ne guest_bad
    mrs x2, elr_el1
    add x3, x21, #0x600
    cmp x2, x3
    b.ne guest_bad
    mrs x2, far_el1
    add x3, x21, #0x700
    cmp x2, x3
    b.ne guest_bad
    add x2, x21, #0
    umov x3, v0.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v0.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #1
    umov x3, v1.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v1.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #2
    umov x3, v2.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v2.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #3
    umov x3, v3.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v3.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #4
    umov x3, v4.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v4.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #5
    umov x3, v5.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v5.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #6
    umov x3, v6.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v6.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #7
    umov x3, v7.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v7.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #8
    umov x3, v8.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v8.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #9
    umov x3, v9.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v9.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #10
    umov x3, v10.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v10.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #11
    umov x3, v11.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v11.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #12
    umov x3, v12.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v12.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #13
    umov x3, v13.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v13.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #14
    umov x3, v14.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v14.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #15
    umov x3, v15.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v15.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #16
    umov x3, v16.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v16.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #17
    umov x3, v17.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v17.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #18
    umov x3, v18.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v18.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #19
    umov x3, v19.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v19.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #20
    umov x3, v20.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v20.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #21
    umov x3, v21.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v21.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #22
    umov x3, v22.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v22.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #23
    umov x3, v23.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v23.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #24
    umov x3, v24.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v24.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #25
    umov x3, v25.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v25.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #26
    umov x3, v26.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v26.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #27
    umov x3, v27.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v27.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #28
    umov x3, v28.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v28.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #29
    umov x3, v29.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v29.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #30
    umov x3, v30.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v30.d[1]
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #31
    umov x3, v31.d[0]
    cmp x3, x2
    b.ne guest_bad
    add x2, x2, #0x80
    umov x3, v31.d[1]
    cmp x3, x2
    b.ne guest_bad
    and x2, x19, #3
    lsl x2, x2, #22
    mrs x3, fpcr
    cmp x3, x2
    b.ne guest_bad
    mov x2, x19
    mrs x3, fpsr
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #0x200
    mrs x3, tpidr_el0
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #0x300
    mrs x3, tpidrro_el0
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #0x400
    mrs x3, tpidr_el1
    cmp x3, x2
    b.ne guest_bad
    add x2, x21, #0x500
    mrs x3, contextidr_el1
    cmp x3, x2
    b.ne guest_bad
    ldr x1, [x0]
    cmp x1, x20
    b.ne guest_bad
    ldr x1, [x0, #8]
    cmp x1, x19
    b.ne guest_bad
    add x20, x20, #1
    str x20, [x0]
    cmp x20, #1
    b.ne guest_resume
    /* Enter an EL0-only page; EL1 continuation is fixed by the relay. */
    mov x2, #0x3000
    msr elr_el1, x2
    mov x2, #0x340
    msr spsr_el1, x2
    eret
guest_after_user:
    mov x0, #0x2000
    add x2, x21, #0x600
    msr elr_el1, x2
    mov x2, #1
    str x2, [x0, #16]
guest_resume:
    cmp x18, x21
    b.ne guest_bad
    cmp x30, x21
    b.ne guest_bad
    b 9b
guest_bad:
    hvc #0x43
    b guest_bad
    .ltorg
.balign 4096
service_vectors:
.rept 8
    b relay_fail
    .balign 128
.endr
    b relay_sync
    .balign 128
.rept 7
    b relay_fail
    .balign 128
.endr
relay_sync:
    mrs x9, spsr_el1
    tst x9, #15
    b.ne relay_fail
    mrs x9, esr_el1
    ldr x10, =0x56000000
    cmp x9, x10
    b.ne relay_fail
    cmp x8, #7 /* SYS_wait4 */
    b.eq relay_submit
    cmp x8, #1 /* fixture exit, local only */
    b.eq relay_local_exit
    /* Unsupported syscall: positive errno and carry set, no AMP frame. */
    mov x0, #78 /* ENOSYS */
    mrs x9, spsr_el1
    orr x9, x9, #(1 << 29)
    msr spsr_el1, x9
    eret
relay_submit:
    hvc #0x44
relay_after_hvc:
    eret
relay_local_exit:
    /* Acknowledged exit: never return to EL0; fixture-only continuation. */
    cbnz x0, relay_fail
    ldr x9, =0x1000 + guest_after_user - guest
    msr elr_el1, x9
    mov x9, #0x345
    msr spsr_el1, x9
    eret
relay_return:
    eret
relay_fail:
    hvc #0x43
    b relay_fail
    .ltorg
.balign 4096
.section .rodata,"a"
passed: .asciz "EXODUS_AWT_SVC_REF_PASS\n"
blocked_msg: .asciz "EXODUS_AWT_SVC_REF_EL1_BLOCKED\n"
failed: .asciz "EXODUS_AWT_SVC_REF_FAIL\n"
.section .bss,"aw",%nobits
.balign 4096
zero_start:
roots: .skip 98304
s1_tables: .skip 98304
data: .skip 32768
contexts: .skip 7680
ready: .skip 32
done: .skip 32
active: .skip 32
yields: .skip 32
samples: .skip 64
quantum: .skip 8
zero_end:
.balign 4096
stacks: .skip 16384
.balign 4096
elf_backing: .skip 98304

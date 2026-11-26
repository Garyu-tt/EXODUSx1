/* QEMU virt only. Cooperative SIMD and thread-state fixtures, not a production scheduler. */
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
    ldr x0, =0x80000001
    msr hcr_el2, x0
    ldr x0, =0x30d00800
    msr sctlr_el1, x0
    mov x0, #0x33ff
    msr cptr_el2, x0
    mov x0, #0x300000
    msr cpacr_el1, x0
    isb
    b enter

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
    mov x2, #0x3c5
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
    b lower_sync
    .balign 128
.rept 7
    b fail
    .balign 128
.endr

lower_sync:
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
    mrs x2, esr_el2
    ldr x3, =0x5a000042
    cmp x2, x3
    b.ne fail
    adr x4, active
    ldr x5, [x4]
    add x6, x5, #1
    ldr x7, [x0, #152]
    cmp x6, x7
    b.ne fail
    ldr x7, [x0, #160]
    adr x8, yields
    ldr x9, [x8]
    lsr x10, x9, #1
    add x10, x10, #1
    cmp x7, x10
    b.ne fail
    add x9, x9, #1
    str x9, [x8]
    cmp x9, #128
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
    adr x0, data_a
    ldr x1, [x0]
    cmp x1, #64
    b.ne fail
    ldr x1, [x0, #8]
    cmp x1, #1
    b.ne fail
    adr x0, data_b
    ldr x1, [x0]
    cmp x1, #64
    b.ne fail
    ldr x1, [x0, #8]
    cmp x1, #2
    b.ne fail
    adr x0, ctx_a
    mov x1, #257
    add x0, x0, #288
    mov x2, #32
10: ldp x3, x4, [x0], #16
    cmp x3, x1
    b.ne fail
    add x5, x1, #0x80
    cmp x4, x5
    b.ne fail
    add x1, x1, #1
    subs x2, x2, #1
    b.ne 10b
    adr x0, ctx_b
    mov x1, #258
    add x0, x0, #288
    mov x2, #32
10: ldp x3, x4, [x0], #16
    cmp x3, x1
    b.ne fail
    add x5, x1, #0x80
    cmp x4, x5
    b.ne fail
    add x1, x1, #1
    subs x2, x2, #1
    b.ne 10b
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
    lsl x2, x19, #22
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
    isb
9:
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
    lsl x2, x19, #22
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
    cmp x18, x21
    b.ne guest_bad
    cmp x30, x21
    b.ne guest_bad
    hvc #0x42
    b 9b
guest_bad:
    hvc #0x43
    b guest_bad
.balign 4096
.section .rodata,"a"
passed: .asciz "EXODUS_SIMD_REF_PASS\n"
blocked_msg: .asciz "EXODUS_SIMD_REF_EL1_BLOCKED\n"
failed: .asciz "EXODUS_SIMD_REF_FAIL\n"
.section .bss,"aw",%nobits
.balign 4096
zero_start:
tables_a: .skip 12288
tables_b: .skip 12288
data_a: .skip 4096
data_b: .skip 4096
ctx_a: .skip 864
ctx_b: .skip 864
active: .skip 8
yields: .skip 8
zero_end:
.balign 16
stack: .skip 4096
stack_top:

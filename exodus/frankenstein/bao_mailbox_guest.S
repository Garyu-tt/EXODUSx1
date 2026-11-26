/* QEMU Bao synthetic guest stress control; no real OS, no physical board.
 * Exercise 64-bit release/acquire publication and acknowledgment in the
 * configured shared window. Stage 1 is explicitly disabled in this fixture.
 * Fresh disposable VM only; do not reset any live mailbox.
 */
.section .text.boot,"ax"
.global _start
_start:
    mrs x0, CurrentEL
    cmp x0, #4
    b.ne halt
    mrs x0, SCTLR_EL1
    tbnz x0, #0, halt
    mov x1, #0xf0000000
    add x1, x1, #0x1000
    add x3, x1, #64
    add x12, x1, #128
    add x13, x1, #136
#ifdef GUEST_A
    /* Both slots initialized exactly once before B attaches. */
    stp xzr, xzr, [x1]
    str xzr, [x1, #16]
    stp xzr, xzr, [x3]
    str xzr, [x3, #16]
    str xzr, [x13]
    mov x4, #1
    stlr x4, [x12]
    mov x5, x1
    mov x6, x3
    mov x14, #0x41
    mov x15, #0x42
#else
1:  ldar x4, [x12]
    cmp x4, #1
    b.ne 1b
    mov x5, x3
    mov x6, x1
    mov x14, #0x42
    mov x15, #0x41
#endif
    add x7, x5, #8
    add x8, x6, #8
    mov x9, #1
round:
    /* Payload distinct for each role and sequence. */
    eor x10, x9, x14, lsl #32
    str x10, [x5, #16]
    stlr x9, [x5]
2:  ldar x11, [x6]
    cmp x11, x9
    b.lo 2b
    b.ne halt
    ldr x11, [x6, #16]
    eor x10, x9, x15, lsl #32
    cmp x11, x10
    b.ne halt
    stlr x9, [x8]
3:  ldar x11, [x7]
    cmp x11, x9
    b.lo 3b
    b.ne halt
    add x9, x9, #1
    cmp x9, #257
    b.lo round
#ifdef GUEST_A
    mov x4, #1
    stlr x4, [x13]
#else
4:  ldar x4, [x13]
    cmp x4, #1
    b.ne 4b
    adr x5, marker
    mov x6, #0x09000000
5:  ldrb w7, [x5], #1
    cbz w7, halt
6:  ldr w8, [x6, #0x18]
    tbnz w8, #5, 6b
    str w7, [x6]
    b 5b
#endif
halt:
    wfe
    b halt
#ifndef GUEST_A
marker: .asciz "TUMOR_BAO_REF: STAGE2_MAILBOX_256_EACH\n"
#endif

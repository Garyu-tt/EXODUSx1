/* QEMU virt synthetic guests only. No Switch addresses or boot payload. */
.section .text.boot,"ax"
.global _start
_start:
    mrs x0, CurrentEL
    cmp x0, #4
    b.ne halt
    mov x1, #0xf0000000
#ifdef GUEST_A
    mov w2, #1
    stlr w2, [x1]
    add x3, x1, #64
1:  ldar w4, [x3]
    cmp w4, #1
    b.ne 1b
    mov w2, #2
    stlr w2, [x1]
#else
    add x3, x1, #64
1:  ldar w4, [x1]
    cmp w4, #1
    b.ne 1b
    mov w2, #1
    stlr w2, [x3]
2:  ldar w4, [x1]
    cmp w4, #2
    b.ne 2b
    adr x5, marker
    mov x6, #0x09000000
3:  ldrb w7, [x5], #1
    cbz w7, halt
4:  ldr w8, [x6, #0x18]
    tbnz w8, #5, 4b
    str w7, [x6]
    b 3b
#endif
halt:
    wfe
    b halt
#ifndef GUEST_A
marker: .asciz "TUMOR_BAO_REF: TWO_EL1_GUESTS_IPC\n"
#endif

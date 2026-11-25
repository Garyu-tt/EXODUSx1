/* Disposable QEMU reference peer. Single transaction, no physical board. */
.section .text.boot,"ax"
.global _start
_start:
    mrs x0, CurrentEL
    cmp x0, #4
    b.ne halt
    mrs x0, SCTLR_EL1
    tbnz x0, #0, halt
    mov x1, #0xf0000000
    add x3, x1, #64
    add x5, x1, #0x2000
    /* Initialize before admitting B to kernel boot. */
    stp wzr, wzr, [x5]
    str wzr, [x5, #8]
    ldr w6, =0x52433831
    str w6, [x5]
    dsb sy
    mov w2, #1
    stlr w2, [x1]
1:  ldar w4, [x3]
    cmp w4, #1
    b.ne 1b
    mov w2, #2
    stlr w2, [x1]
    ldr w6, =0x52433832
2:  ldr w7, [x5, #4]
    dsb sy
    cbz w7, 2b
    cmp w7, w6
    b.ne halt
    ldr w6, =0x52433833
    str w6, [x5, #8]
    dsb sy
halt:
    wfe
    b halt

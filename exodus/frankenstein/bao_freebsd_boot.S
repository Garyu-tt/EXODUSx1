.section .text.boot,"ax"
.global _start
_start:
    mrs x0, CurrentEL
    cmp x0, #4
    b.ne halt
    mov x1, #0xf0000000
    add x3, x1, #64
#ifdef TUMOR_HEADLESS_PEER
    /* Role A mirrors the established handshake and never touches UART. */
    mov w2, #1
    stlr w2, [x1]
1:  ldar w4, [x3]
    cmp w4, #1
    b.ne 1b
    mov w2, #2
    stlr w2, [x1]
    b enter
#else
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
    cbz w7, enter
4:  ldr w8, [x6, #0x18]
    tbnz w8, #5, 4b
    str w7, [x6]
    b 3b
#endif
enter:
#ifdef TUMOR_FREEBSD_NATIVE_METADATA
    /* QEMU guest bundle slot contains a FreeBSD kernel virtual pointer. */
    ldr x0, =0x62200000
    ldr x0, [x0]
    cbz x0, halt
#else
    mov x0, #0x62000000
#endif
    mov x1, xzr
    mov x2, xzr
    mov x3, xzr
    ldr x4, =0x60200800
    br x4
halt:
    wfe
    b halt
marker: .asciz "TUMOR_BAO_REF: FREEBSD_HANDOFF_AFTER_PEER_IPC\n"

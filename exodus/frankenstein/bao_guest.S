/* QEMU virt synthetic guests only. No Switch addresses or boot payload. */
.section .text.boot,"ax"
.global _start
_start:
    mrs x0, CurrentEL
    cmp x0, #4
    b.ne halt
    mov x1, #0xf0000000
#ifdef GUEST_A
#ifdef TUMOR_NC_EXCHANGE_CONTROL
    /* Separate fixed test lanes; initialized before publishing boot-ready. */
    add x16, x1, #0x2000
    ldr w17, =0x544d5238
    str w17, [x16]
    str wzr, [x16, #64]
    str wzr, [x16, #128]
    dmb sy
#endif
    mov w2, #1
    stlr w2, [x1]
    add x3, x1, #64
1:  ldar w4, [x3]
    cmp w4, #1
    b.ne 1b
    mov w2, #2
    stlr w2, [x1]
#ifdef TUMOR_NC_EXCHANGE_CONTROL
nc_wait:
    ldr w17, [x16, #64]
    dmb sy
    cbz w17, nc_wait
    ldr w18, =0x52455338
    cmp w17, w18
    b.ne halt
    ldr w17, =0x41434b38
    str w17, [x16, #128]
    dmb sy
    b halt
#endif
#ifdef TUMOR_RESIDENT_PEER
    /* QEMU register-observation fixture, no new physical mappings. */
    ldr x10, =0x544d525045455231
    mov x9, #0
peer_tick:
    add x9, x9, #1
    yield
    b peer_tick
#endif
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

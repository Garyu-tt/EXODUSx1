.section .text,"ax"
.global _start
_start:
    mov x2,#0x5000
    ldr x3,[x2]
    ldr x4,=0x13579bdf2468ace0
    cmp x3,x4
    b.ne bad
    ldr w3,[x2,#8]
    cbnz w3,bad
    ldr w11,[x2,#12]
    cbz w11,bad
    tbnz w11,#31,bad
    str wzr,[x2,#12]
    /* Bad copyout: executable page, no wire request. */
    mov x0,x11
    mov x1,#0x3000
    mov x2,#1
    mov x3,#0
    mov x8,#7
    svc #0
    b.cc bad
    cmp x0,#14
    b.ne bad
    /* Exact native PID provisioned before EL0 starts. */
    mov x0,x11
    mov x1,#0x5008
    mov x2,#1
    mov x3,#0
    mov x8,#7
    svc #0
    b.cs bad
    cmp x0,x11
    b.ne bad
    mov x2,#0x5000
    ldr w3,[x2,#8]
    add x4,x19,#22
    lsl x4,x4,#8
    cmp x3,x4
    b.ne bad
    ldr w3,[x2,#12]
    cbnz w3,bad
    ldr x3,[x2]
    ldr x4,=0x13579bdf2468ace0
    cmp x3,x4
    b.ne bad
    /* Restore original ELF verifier's identity marker after copyout check. */
    str x19,[x2,#8]
    mov x0,#0
    mov x8,#1
    svc #0
bad:
    svc #0x46
    b bad
.ltorg
.section .data,"aw"
.quad 0x13579bdf2468ace0
.section .bss,"aw",%nobits
.space 8

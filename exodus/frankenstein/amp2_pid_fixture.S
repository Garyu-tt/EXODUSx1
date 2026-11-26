.section .text,"ax"
.global _start
_start:
    mov x2, #0x5000
    ldr x3, [x2]
    ldr x4, =0x13579bdf2468ace0
    cmp x3, x4
    b.ne bad
    ldr x3, [x2, #8]
    cbnz x3, bad
    str x19, [x2, #8]
    /* Unsupported number must return ENOSYS and carry set. */
    mov x8, #4095
    svc #0
    b.cc bad
    cmp x0, #78
    b.ne bad
    /* SYS_getpid: success clears carry and returns positive pid_t in x0. */
    mov x8, #20
    svc #0
    b.cs bad
    cmp x0, #0
    b.le bad
    lsr x2, x0, #31
    cbnz x2, bad
    mov x2, #0x5000
    ldr x3, [x2, #8]
    cmp x3, x19
    b.ne bad
    ldr x3, [x2]
    ldr x4, =0x13579bdf2468ace0
    cmp x3, x4
    b.ne bad
    /* SYS_exit: EL0 must never resume after acknowledged exit. */
    mov x0, #23
    mov x8, #1
    svc #0
bad:
    svc #0x46
    b bad
.ltorg
.section .data,"aw"
.quad 0x13579bdf2468ace0
.section .bss,"aw",%nobits
.space 8

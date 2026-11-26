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
    mov x8, #1
    mov x1, #0
    svc #0x44
    ldr x2, =0x1234567887654300
    add x2, x2, x19
    cmp x8, x2
    b.ne bad
    mov x8, #2
    mov x1, #23
    svc #0x44
    cbnz x8, bad
    mov x2, #0x5000
    ldr x3, [x2, #8]
    cmp x3, x19
    b.ne bad
    ldr x3, [x2]
    ldr x4, =0x13579bdf2468ace0
    cmp x3, x4
    b.ne bad
    svc #0x45
bad:
    svc #0x46
    b bad
.ltorg
.section .data,"aw"
.quad 0x13579bdf2468ace0
.section .bss,"aw",%nobits
.space 8

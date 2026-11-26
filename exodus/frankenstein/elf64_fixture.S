.section .text,"ax"
.global _start
_start:
    mov x0,#23
    mov x8,#93
    svc #0
    b .
.section .data,"aw"
.quad 0x1234567887654321
.section .bss,"aw",%nobits
.space 8192

/* Synthetic ARM-state program; no hardware registers or FreeBSD code. */
.syntax unified
.cpu cortex-a9
.arm
.section .text,"ax",%progbits
.global _start
_start:
    movw r0, #0x1000
    movt r0, #1
    ldr r1, [r0]
    ldr r2, [r0, #4]
    add r1, r1, r2
    add r1, r1, #1
    str r1, [r0, #4]
1:  b 1b
.section .data,"aw",%progbits
.align 2
.word 41
.section .bss,"aw",%nobits
.align 2
.space 4

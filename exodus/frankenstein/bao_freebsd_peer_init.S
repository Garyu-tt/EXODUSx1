/* QEMU-only FreeBSD PID 1 liveness fixture. No console or device access. */
.section .text,"ax"
.global _start
_start:
    mov x9, #0
again:
    mov x8, #20
    svc #0
    b.cs fail
    cmp x0, #1
    b.ne fail
    ldr x10, =0x544d525045455232
    add x9, x9, #1
    mov x11, #1024
delay:
    subs x11, x11, #1
    b.ne delay
    b again
fail:
    mov x10, xzr
    b fail
.section .note.ABI-tag,"a",%note
.balign 4
.long 8, 4, 1
.asciz "FreeBSD"
.balign 4
.long 1501000
.balign 4

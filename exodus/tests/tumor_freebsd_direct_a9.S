/* QEMU-only Linux-style ARM boot ABI handoff. */
.syntax unified
.cpu cortex-a9
.arm
.global _start
_start:
    cpsid if
    mov r0,#0
    mvn r1,#0
    ldr r2,=0x60040000
    mov r3,#0
    ldr r4,=0x61000200
    bx r4
.ltorg

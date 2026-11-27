/* Bare-metal QEMU vexpress-a9 reference, not Tegra/ADSP boot code.
 * Synthetic machine RAM only. ARMv7 short-descriptor 1 MiB sections.
 * No physical console interaction or device register programming.
 */
.syntax unified
.cpu cortex-a9
.arm
.section .text,"ax",%progbits
.balign 32
.global _start
_start:
    b boot
    b fail
    b fail
    b fail
    b data_abort
    b fail
    b fail
    b fail
boot:
    cpsid if
    ldr r0, =0x60020000
    mov r1, #0
    mov r2, #4096
1:  str r1, [r0], #4
    subs r2, r2, #1
    bne 1b
    ldr r0, =0x60020000
    ldr r1, =0x60001402
    ldr r2, =0x1800
    str r1, [r0, r2]
    ldr r2, =0x3000
    str r1, [r0, r2]
    mov r1, #0
    mcr p15, 0, r1, c2, c0, 2
    mcr p15, 0, r0, c2, c0, 0
    mov r1, #1
    mcr p15, 0, r1, c3, c0, 0
    mov r1, #0
    dsb sy
    mcr p15, 0, r1, c8, c7, 0
    dsb sy
    isb
    mrc p15, 0, r1, c1, c0, 0
    bic r1, r1, #(1 << 28)
    bic r1, r1, #(1 << 29)
    bic r1, r1, #(1 << 13)
    orr r1, r1, #(1 << 23)
    orr r1, r1, #1
    mcr p15, 0, r1, c1, c0, 0
    isb
    adr r0, high_entry
    add r0, r0, #0x60000000
    bx r0
high_entry:
    mov r0, pc
    lsr r0, r0, #24
    cmp r0, #0xc0
    bne fail
    ldr r0, =0x60030000
    ldr r1, =0xc0030000
    ldr r2, =0x12345678
    str r2, [r0]
    dsb sy
    ldr r3, [r1]
    cmp r2, r3
    bne fail
    mvn r2, r2
    str r2, [r1]
    dsb sy
    ldr r3, [r0]
    cmp r2, r3
    bne fail
    ldr r0, =0xc0010000
    mcr p15, 0, r0, c12, c0, 0
    isb
    mov r4, #0
    ldr r0, =0xd0000000
    ldr r1, [r0]
    cmp r4, #1
    bne fail
    adr r1, pass_msg
    mov r0, #4
    svc 0x123456
    adr r1, exit_pass
    mov r0, #0x20
    svc 0x123456
    b .
data_abort:
    mrc p15, 0, r5, c6, c0, 0
    ldr r6, =0xd0000000
    cmp r5, r6
    bne fail
    mrc p15, 0, r5, c5, c0, 0
    and r5, r5, #15
    cmp r5, #5
    bne fail
    mov r4, #1
    subs pc, lr, #4
fail:
    adr r1, fail_msg
    mov r0, #4
    svc 0x123456
    adr r1, exit_fail
    mov r0, #0x20
    svc 0x123456
    b .
.balign 4
exit_pass: .word 0x20026, 0
exit_fail: .word 0x20026, 1
pass_msg: .asciz "PASS: QEMU Cortex-A9 MMU identity/high alias and unmapped section abort\n"
fail_msg: .asciz "FAIL: Cortex-A9 MMU reference\n"
.balign 4
.ltorg

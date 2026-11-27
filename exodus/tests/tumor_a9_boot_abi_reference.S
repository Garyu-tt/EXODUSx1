/* Synthetic handoff using the Linux ARM boot register convention
 * accepted by FreeBSD ARMv7. No FreeBSD kernel or real ADSP hardware.
 */
.syntax unified
.cpu cortex-a9
.arm
.section .text,"ax",%progbits
.global _start
_start:
    cpsid if
    mov r0, #0
    mvn r1, #0
    ldr r2, =0x60040000
    mov r3, #0
    b receiver
receiver:
    cmp r0, #0
    bne fail
    cmn r1, #1
    bne fail
    ldr r4, =0x60040000
    cmp r2, r4
    bne fail
    tst r2, #7
    bne fail
    cmp r3, #0
    bne fail
    ldr r4, [r2]
    rev r4, r4
    ldr r5, =0xd00dfeed
    cmp r4, r5
    bne fail
    ldr r4, [r2, #4]
    rev r4, r4
    cmp r4, #40
    blo fail
    cmp r4, #0x10000
    bhi fail
    ldr r5, [r2, #20]
    rev r5, r5
    cmp r5, #17
    bne fail
    ldr r5, [r2, #8]
    rev r5, r5
    cmp r5, #40
    blo fail
    cmp r5, r4
    bhs fail
    ldr r6, [r2, #36]
    rev r6, r6
    sub r7, r4, r5
    cmp r6, #4
    blo fail
    cmp r6, r7
    bhi fail
    ldr r6, [r2, r5]
    rev r6, r6
    cmp r6, #1
    bne fail
    adr r1, pass_msg
    mov r0, #4
    svc 0x123456
    adr r1, exit_pass
    mov r0, #0x20
    svc 0x123456
    b .
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
pass_msg: .asciz "PASS: ARM boot ABI registers and FDT header accepted\n"
fail_msg: .asciz "FAIL: ARM boot ABI or FDT rejected\n"
.balign 4
.ltorg

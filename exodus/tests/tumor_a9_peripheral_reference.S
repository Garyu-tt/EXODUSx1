/* QEMU vexpress-a9 ONLY: GIC, private timer and PL011 console.
 * Addresses are for the emulated reference board, NOT Switch/ADSP.
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
    b fail
    b fail
    b irq
    b fail
boot:
    cpsid if
    adr r0, _start
    mcr p15, 0, r0, c12, c0, 0
    mrc p15, 0, r1, c1, c0, 0
    bic r1, r1, #(1 << 13)
    mcr p15, 0, r1, c1, c0, 0
    isb
    ldr r0, =0x1e001000
    ldr r1, =0x1e000100
    ldr r2, =0x1e000600
    mov r3, #0
    str r3, [r2, #8]
    mov r3, #1
    str r3, [r2, #12]
    str r3, [r0]
    str r3, [r1]
    mov r3, #255
    str r3, [r1, #4]
    mov r3, #(1 << 29)
    str r3, [r0, #0x100]
    mov r4, #0
    mov r5, #1
    dsb sy
again:
    ldr r3, =100000
    str r3, [r2]
    mov r3, #5
    str r3, [r2, #8]
    dsb sy
    cpsie i
1:  cmp r4, r5
    blo 1b
    cpsid i
    cmp r4, r5
    bne fail
    ldr r3, [r2, #12]
    tst r3, #1
    bne fail
    add r5, r5, #1
    cmp r5, #4
    bne again
    /* PL011 UART0 polling console. */
    ldr r0, =0x10009000
    mov r1, #0
    str r1, [r0, #0x30]
    mov r1, #13
    str r1, [r0, #0x24]
    mov r1, #1
    str r1, [r0, #0x28]
    mov r1, #0x60
    str r1, [r0, #0x2c]
    ldr r1, =0x301
    str r1, [r0, #0x30]
    adr r1, uart_msg
2:  ldrb r2, [r1], #1
    cmp r2, #0
    beq uart_done
3:  ldr r3, [r0, #0x18]
    tst r3, #0x20
    bne 3b
    str r2, [r0]
    b 2b
uart_done:
    adr r1, pass_msg
    mov r0, #4
    svc 0x123456
    adr r1, exit_pass
    mov r0, #0x20
    svc 0x123456
    b .
irq:
    /* Only bank-independent scratch registers and the explicit counter.
     * Main loop preserves r8-r12; no stack is required in this fixture.
     */
    ldr r8, =0x1e000100
    ldr r9, [r8, #0xc]
    ubfx r10, r9, #0, #10
    cmp r10, #29
    bne fail
    ldr r11, =0x1e000600
    ldr r12, [r11, #12]
    tst r12, #1
    beq fail
    mov r12, #0
    str r12, [r11, #8]
    mov r12, #1
    str r12, [r11, #12]
    dsb sy
    str r9, [r8, #0x10]
    dsb sy
    add r4, r4, #1
    subs pc, lr, #4
fail:
    cpsid if
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
uart_msg: .asciz "PASS: PL011 early console\n"
pass_msg: .asciz "PASS: Cortex-A9 private timer GIC PPI 29 delivered three IRQs\n"
fail_msg: .asciz "FAIL: QEMU A9 peripheral reference\n"
.balign 4
.ltorg

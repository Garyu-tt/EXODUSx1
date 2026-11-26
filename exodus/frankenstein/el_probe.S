/* QEMU virt reference only. Never deploy to Nintendo hardware. */
.section .text.boot,"ax"
.global _start
_start:
    mrs x0, CurrentEL
    lsr x0, x0, #2
    and x0, x0, #3
    cmp x0, #2
    b.eq el2
    cmp x0, #1
    b.eq el1
    adr x1, unknown
    b report
el2:
    adr x1, good
    b report
el1:
    adr x1, blocked
report:
    /* PL011 UART on QEMU virt, not Tegra. */
    mov x2, #0x09000000
next:
    ldrb w3, [x1], #1
    cbz w3, done
wait_uart:
    ldr w4, [x2, #0x18]
    tbnz w4, #5, wait_uart
    str w3, [x2]
    b next
done:
    wfe
    b done
.section .rodata,"a"
good: .asciz "TUMOR_REF: EL2\n"
blocked: .asciz "TUMOR_REF: EL1_BLOCKED\n"
unknown: .asciz "TUMOR_REF: UNEXPECTED_EL\n"

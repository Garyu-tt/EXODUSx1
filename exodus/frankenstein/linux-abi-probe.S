/* Linux AArch64 syscall-only test, not the Steam client.
 * Build once at ARMv8.0 baseline and once with a single ARMv8.1 LSE operation.
 */
.section .note.ABI-tag,"a",%note
.balign 4
.long 4,16,1
.asciz "GNU"
.balign 4
.long 0,3,2,0
.text
.global _start
_start:
#ifdef TEST_LSE
    adrp x1, counter
    add x1, x1, :lo12:counter
    mov x2, #1
    ldadd x2, x3, [x1]
    cbnz x3, fail
    ldr x3, [x1]
    cmp x3, #1
    b.ne fail
#endif
    mov x0, #1
    adr x1, message
    mov x2, #message_end-message
    mov x8, #64
    svc #0
    cmp x0, #message_end-message
    b.ne fail
    mov x0, #0
    b exit
fail:
    mov x0, #1
exit:
    mov x8, #93
    svc #0
    brk #0
.section .rodata
message: .ascii "TUMOR_LINUX_ABI_OK\n"
message_end:
.data
.balign 8
counter: .quad 0
.section .note.GNU-stack,"",%progbits

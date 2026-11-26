/* QEMU virt only, not a Switch boot payload. Single trusted EL0 fixture,
 * no MMU/stage-2 isolation, no Horizon or physical AMP authority. */
.section .text.boot,"ax"
.global _start
_start:
    mrs x0, CurrentEL
    cmp x0, #8
    b.eq setup
    cmp x0, #4
    b.ne fail
    adr x1, blocked
    b report
setup:
    msr daifset, #15
    msr spsel, #1
    adr x0, monitor_stack_top
    mov sp, x0
    adr x0, vectors
    msr vbar_el2, x0
    /* RW=1: AArch64 lower EL; TGE=1: route EL0 exceptions to EL2.
     * This reference is NOT a guest EL1 coexistence configuration. */
    mov x0, #0x88000000
    msr hcr_el2, x0
    isb
    adr x0, user_stack_top
    msr sp_el0, x0
    adr x0, user_entry
    msr elr_el2, x0
    mov x0, #0x3c0
    msr spsr_el2, x0
    eret

.balign 2048
vectors:
.rept 8
    b fail
    .balign 128
.endr
    b lower_sync
    .balign 128
.rept 7
    b fail
    .balign 128
.endr

lower_sync:
    stp x9, x10, [sp, #-32]!
    stp x11, x12, [sp, #16]
    mrs x9, spsr_el2
    and x9, x9, #15
    cbnz x9, fail
    mrs x9, esr_el2
    lsr x10, x9, #26
    cmp x10, #0x15
    b.ne fail
    and x9, x9, #0xffff
    cbnz x9, fail
    /* EXODUS reference opcodes, not FreeBSD syscall numbers. */
    cmp x8, #1
    b.eq identity
    cmp x8, #2
    b.eq finish
    mov x0, #-1
    b resume
identity:
    mov x0, #0x1234
resume:
    ldp x11, x12, [sp, #16]
    ldp x9, x10, [sp], #32
    eret
finish:
    cmp x0, #23
    b.ne fail
    adr x1, passed
    b report

user_entry:
    mov x19, #0x5678
    mov x9, #0x1357
    mov x8, #1
    svc #0
    mov x1, #0x1234
    cmp x0, x1
    b.ne user_fail
    mov x1, #0x1357
    cmp x9, x1
    b.ne user_fail
    mov x1, #0x5678
    cmp x19, x1
    b.ne user_fail
    mov x8, #99
    svc #0
    cmn x0, #1
    b.ne user_fail
    mov x0, #23
    mov x8, #2
    svc #0
user_fail:
    mov x0, #1
    mov x8, #2
    svc #0
    b user_fail

fail:
    adr x1, failed
report:
    /* PL011, QEMU virt only. */
    mov x2, #0x09000000
1:  ldrb w3, [x1], #1
    cbz w3, 3f
2:  ldr w4, [x2, #0x18]
    tbnz w4, #5, 2b
    str w3, [x2]
    b 1b
3:  wfe
    b 3b
.section .rodata,"a"
passed: .asciz "EXODUS_EL2_REF_PASS\n"
blocked: .asciz "EXODUS_EL2_REF_EL1_BLOCKED\n"
failed: .asciz "EXODUS_EL2_REF_FAIL\n"
.section .bss,"aw",%nobits
.balign 16
monitor_stack: .skip 4096
monitor_stack_top:
user_stack: .skip 4096
user_stack_top:

/* FreeBSD ARM EABI guest-only mmap/fork/wait4 smoke test; no hardware access. */
.syntax unified
.cpu cortex-a9
.arm
.section .note.tag,"a",%note
.balign 4
.long 8,4,1
.asciz "FreeBSD"
.balign 4
.long 1501000
.text
.global _start
_start:
    mov r8,#1
    bl run_case
    mov r8,#2
    bl run_case
    mov r0,#1
    ldr r1,=message
    ldr r2,=message_end-message
    mov r7,#4
    svc #0
    bcs fail
    ldr r2,=message_end-message
    cmp r0,r2
    bne fail
    mov r0,#0
    b exit
run_case:
    push {r4-r6,lr}
    sub sp,sp,#16
    mvn r0,#0
    str r0,[sp]
    mov r0,#0
    str r0,[sp,#4]
    str r0,[sp,#8]
    str r0,[sp,#12]
    mov r1,#4096
    mov r2,#3
    orr r3,r8,#0x1000
    movw r7,#477
    svc #0
    bcs fail
    add sp,sp,#16
    mov r4,r0
    mov r0,#0
    str r0,[r4]
    str r0,[r4,#4]
    mov r7,#2
    svc #0
    bcs fail
    cmp r0,#0
    beq child
    mov r5,r0
    sub sp,sp,#8
    mov r0,r5
    mov r1,sp
    mov r2,#0
    mov r3,#0
    mov r7,#7
    svc #0
    bcs fail
    cmp r0,r5
    bne fail
    ldr r0,[sp]
    add sp,sp,#8
    cmp r0,#0
    bne fail
    dmb ish
    ldr r0,[r4]
    ldr r1,[r4,#4]
    cmp r8,#1
    bne private_check
    cmp r0,#42
    bne fail
    cmp r1,#1
    bne fail
    b unmap
private_check:
    orrs r0,r0,r1
    bne fail
unmap:
    mov r0,r4
    mov r1,#4096
    mov r7,#73
    svc #0
    bcs fail
    pop {r4-r6,pc}
child:
    mov r0,#42
    str r0,[r4]
    dmb ish
    mov r0,#1
    str r0,[r4,#4]
    dmb ish
    mov r0,#0
    b exit
fail:
    mov r0,#1
exit:
    mov r7,#1
    svc #0
    udf #0
message:
    .ascii "EXODUS_GUEST_SHM_PASS\n"
message_end:

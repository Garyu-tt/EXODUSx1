/* R61: actual FreeBSD A9 process ping-pong, coherent guest only. */
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
    sub sp,sp,#16
    mvn r0,#0
    str r0,[sp]
    mov r0,#0
    str r0,[sp,#4]
    str r0,[sp,#8]
    str r0,[sp,#12]
    mov r1,#4096
    mov r2,#3
    movw r3,#0x1001
    movw r7,#477
    svc #0
    bcs fail
    add sp,sp,#16
    mov r4,r0
    mov r0,#0
    str r0,[r4]
    str r0,[r4,#64]
    mov r7,#2
    svc #0
    bcs fail
    mov r10,r0
    mov r5,#1
    movw r6,#1000
    cmp r0,#0
    beq child_loop
parent_loop:
    str r5,[r4,#128]
    mvn r0,r5
    str r0,[r4,#132]
    dmb ish
    str r5,[r4]
    add r8,r4,#64
    bl wait_token
    ldr r0,[r4,#192]
    add r1,r5,#42
    cmp r0,r1
    bne fail
    ldr r0,[r4,#196]
    mvn r1,r1
    cmp r0,r1
    bne fail
    add r5,r5,#1
    cmp r5,r6
    bls parent_loop
    sub sp,sp,#8
    mov r0,r10
    mov r1,sp
    mov r2,#0
    mov r3,#0
    mov r7,#7
    svc #0
    bcs fail
    cmp r0,r10
    bne fail
    ldr r0,[sp]
    add sp,sp,#8
    cmp r0,#0
    bne fail
    mov r0,r4
    mov r1,#4096
    mov r7,#73
    svc #0
    bcs fail
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
child_loop:
    mov r8,r4
    bl wait_token
    ldr r0,[r4,#128]
    cmp r0,r5
    bne fail
    ldr r0,[r4,#132]
    mvn r1,r5
    cmp r0,r1
    bne fail
    add r0,r5,#42
    str r0,[r4,#192]
    mvn r0,r0
    str r0,[r4,#196]
    dmb ish
    str r5,[r4,#64]
    add r5,r5,#1
    cmp r5,r6
    bls child_loop
    mov r0,#0
    b exit
/* Bounded polling with scheduler yield; never accept skipped token. */
wait_token:
    movw r9,#0x86a0
    movt r9,#1
1:
    ldr r0,[r8]
    cmp r0,r5
    beq 2f
    bhi fail
    subs r9,r9,#1
    beq fail
    movw r7,#331
    svc #0
    bcs fail
    b 1b
2:
    dmb ish
    bx lr
fail:
    mov r0,#1
exit:
    mov r7,#1
    svc #0
    udf #0
message:
    .ascii "EXODUS_PINGPONG_1000_PASS\n"
message_end:

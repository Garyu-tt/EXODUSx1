/* QEMU-only: map last page of disposable file-backed emulated RAM.
 * Page is excluded from guest allocator by shortening FDT memory range.
 * Never run on physical hardware. SHARED_PA is set by the CI fixture.
 */
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
    ldr r0,=path
    mov r1,#2
    mov r2,#0
    mov r7,#5
    svc #0
    bcs fail
    mov r5,r0
    sub sp,sp,#16
    str r5,[sp]
    mov r0,#0
    str r0,[sp,#4]
    ldr r1,=SHARED_PA
    str r1,[sp,#8]
    str r0,[sp,#12]
    mov r1,#4096
    mov r2,#3
    mov r3,#1
    movw r7,#477
    svc #0
    bcs fail
    add sp,sp,#16
    mov r4,r0
    mov r11,#0
session:
    mov r0,r11
    bl guest_session_init
    cmp r0,#1
    bne fail
    ldr r6,=0xfffffe0b
    mov r10,#0
    movw r8,#1000
round:
    adds r6,r6,#1
    adc r10,r10,#0
    movw r9,#0xffff
    movt r9,#0x0010
poll:
    ldr r0,[r4]
    cmp r0,r6
    beq request_ready
    sub r1,r6,#1
    cmp r0,r1
    bne reject
    subs r9,r9,#1
    beq fail
    movw r7,#331
    svc #0
    bcs fail
    b poll
request_ready:
    dmb ish
    mov r0,r4
    mov r1,r6
    mov r2,r10
    mov r3,r11
    bl guest_reply
    cmp r0,#0
    beq reject
    cmp r0,#1
    bne fail
    dmb ish
    str r6,[r4,#192]
    dmb ish
    subs r8,r8,#1
    bne round
    mov r0,r4
    mov r1,#4096
    mov r7,#73
    svc #0
    bcs fail
    mov r0,r5
    mov r7,#6
    svc #0
    bcs fail
    mov r0,#1
    ldr r1,=msg
    ldr r2,=end-msg
    mov r7,#4
    svc #0
    bcs fail
    ldr r2,=end-msg
    cmp r0,r2
    bne fail
    mov r0,#0
    b exit
reject:
    cmp r11,#0
    bne fail
    /* Quiesce: never touch data until host reset command is visible. */
    dmb ish
    mov r0,#1
    str r0,[r4,#320]
    dmb ish
    movw r9,#0xffff
    movt r9,#0x0010
reset_wait:
    ldr r0,[r4,#256]
    cmp r0,#1
    beq reset_ready
    cmp r0,#0
    bne fail
    subs r9,r9,#1
    beq fail
    movw r7,#331
    svc #0
    bcs fail
    b reset_wait
reset_ready:
    dmb ish
    ldr r0,[r4]
    ldr r1,=0xfffffe0b
    cmp r0,r1
    bne fail
    /* Guest owns reply and ACK; host owns request and publication. */
    mov r0,#0
    str r0,[r4,#128]
    str r0,[r4,#132]
    str r0,[r4,#136]
    str r0,[r4,#140]
    str r0,[r4,#144]
    str r0,[r4,#148]
    str r1,[r4,#192]
    mov r11,#1
    dmb ish
    mov r0,#2
    str r0,[r4,#320]
    dmb ish
    b session
fail:
    mov r0,#1
exit:
    mov r7,#1
    svc #0
    udf #0
.balign 4
.global guest_clock
guest_clock:
    push {r7,lr}
    mov r1,r0
    mov r0,#4
    mov r7,#232
    svc #0
    movcs r0,#1
    movcc r0,#0
    pop {r7,pc}
path: .asciz "/dev/mem"
msg: .ascii "EXODUS_GUEST_NATIVE_CODEC_PASS\n"
end:

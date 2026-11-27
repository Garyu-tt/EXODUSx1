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
    mov r6,#1
    movw r8,#1000
round:
    movw r9,#0xffff
    movt r9,#0x0010
poll:
    ldr r0,[r4]
    cmp r0,r6
    beq request_ready
    bhi fail
    subs r9,r9,#1
    beq fail
    movw r7,#331
    svc #0
    bcs fail
    b poll
request_ready:
    dmb ish
    ldr r0,[r4,#64]
    cmp r0,r6
    bne fail
    ldr r0,[r4,#68]
    mvn r1,r6
    cmp r0,r1
    bne fail
    add r0,r6,#42
    str r0,[r4,#128]
    mvn r0,r0
    str r0,[r4,#132]
    dmb ish
    str r6,[r4,#192]
    dmb ish
    add r6,r6,#1
    cmp r6,r8
    bls round
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
fail:
    mov r0,#1
exit:
    mov r7,#1
    svc #0
    udf #0
path: .asciz "/dev/mem"
msg: .ascii "EXODUS_GUEST_RAM_1000_PASS\n"
end:

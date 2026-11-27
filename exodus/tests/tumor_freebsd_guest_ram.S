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
    ldr r0,[r4]
    ldr r1,=0x544d5231
    cmp r0,r1
    bne fail
    dmb ish
    ldr r0,[r4,#64]
    ldr r1,=0x12345678
    cmp r0,r1
    bne fail
    ldr r0,[r4,#68]
    mvn r1,r1
    cmp r0,r1
    bne fail
    ldr r0,=0x87654321
    str r0,[r4,#128]
    mvn r0,r0
    str r0,[r4,#132]
    dmb ish
    mov r0,#1
    str r0,[r4,#192]
    dmb ish
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
msg: .ascii "EXODUS_GUEST_RAM_PASS\n"
end:

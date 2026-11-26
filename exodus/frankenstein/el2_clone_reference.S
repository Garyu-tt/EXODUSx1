/* QEMU A57: private clone fixture, independent of AWS1. */
.section .text.boot,"ax"
.global _start
_start:
    mrs x0,CurrentEL
    cmp x0,#8
    b.ne blocked
    msr daifset,#15
    msr spsel,#1
    adr x0,stack_top
    mov sp,x0
    adr x0,vectors
    msr vbar_el2,x0
    adr x0,zero_start
    adr x1,zero_end
1:  str xzr,[x0],#8
    cmp x0,x1
    b.lo 1b
    adr x0,roots
    adr x1,s1roots
    adr x2,pages
    bl map
    adr x0,roots
    add x0,x0,#3,lsl #12
    adr x1,s1roots
    add x1,x1,#3,lsl #12
    adr x2,pages
    add x2,x2,#1,lsl #12
    bl map
    ldr x0,=0x80003559
    msr vtcr_el2,x0
    ldr x0,=0x80000001
    msr hcr_el2,x0
    ldr x0,=0x80993519
    msr tcr_el1,x0
    mov x0,#0xff
    msr mair_el1,x0
    mov x0,#0x3000
    msr vbar_el1,x0
    msr sp_el0,x0
    msr sp_el1,x0
    mov x0,#0
    bl switch_root
    ldr x0,=0x30d00801
    msr sctlr_el1,x0
    isb
    mov x0,#0x1000
    msr elr_el2,x0
    mov x0,#0x3c0
    msr spsr_el2,x0
    eret
    .ltorg
/* x0 S2 root, x1 S1 root, x2 private data page */
map:
    add x3,x0,#1,lsl #12
    add x4,x0,#2,lsl #12
    orr x5,x3,#3
    str x5,[x0,#8]
    orr x5,x4,#3
    str x5,[x3]
    adr x5,user_code
    mov x6,#0x77f
    orr x5,x5,x6
    str x5,[x4,#8]
    mov x7,#0x7ff
    orr x5,x2,x7
    orr x5,x5,#(1<<54)
    str x5,[x4,#16]
    adr x5,service_vectors
    orr x5,x5,x6
    str x5,[x4,#24]
    orr x6,x6,#(1<<54)
    orr x5,x1,x6
    str x5,[x4,#32]
    add x5,x1,#1,lsl #12
    orr x5,x5,x6
    str x5,[x4,#40]
    add x5,x1,#2,lsl #12
    orr x5,x5,x6
    str x5,[x4,#48]
    ldr x5,=0x40005003
    str x5,[x1]
    add x3,x1,#1,lsl #12
    ldr x5,=0x40006003
    str x5,[x3]
    add x3,x1,#2,lsl #12
    ldr x5,=0x00200000400017c3
    str x5,[x3,#8]
    ldr x5,=0x0060000040002743
    str x5,[x3,#16]
    ldr x5,=0x0040000040003783
    str x5,[x3,#24]
    ret
    .ltorg
/* x0 0 parent / 1 child. Private VMID and ASID, same guest VA/IPA. */
switch_root:
    adr x1,roots
    mov x2,#12288
    madd x1,x0,x2,x1
    add x2,x0,#1
    orr x1,x1,x2,lsl #48
    dsb ish
    msr vttbr_el2,x1
    isb
    ldr x1,=0x40004000
    orr x1,x1,x2,lsl #48
    msr ttbr0_el1,x1
    isb
    tlbi vmalls12e1is
    dsb ish
    isb
    ret
    .ltorg
.balign 2048
vectors:
.rept 8
    b fail
    .balign 128
.endr
    b trap
    .balign 128
.rept 7
    b fail
    .balign 128
.endr
trap:
    /* Save every GPR before using scratch registers. */
    sub sp,sp,#256
    stp x0,x1,[sp,#0]
    stp x2,x3,[sp,#16]
    stp x4,x5,[sp,#32]
    stp x6,x7,[sp,#48]
    stp x8,x9,[sp,#64]
    stp x10,x11,[sp,#80]
    stp x12,x13,[sp,#96]
    stp x14,x15,[sp,#112]
    stp x16,x17,[sp,#128]
    stp x18,x19,[sp,#144]
    stp x20,x21,[sp,#160]
    stp x22,x23,[sp,#176]
    stp x24,x25,[sp,#192]
    stp x26,x27,[sp,#208]
    stp x28,x29,[sp,#224]
    str x30,[sp,#240]
    mrs x0,esr_el2
    ldr x1,=0x5a000044
    cmp x0,x1
    b.ne fail
    mrs x0,spsr_el2
    and x0,x0,#15
    cmp x0,#5
    b.ne fail
    mrs x0,esr_el1
    ldr x1,=0x56000000
    cmp x0,x1
    b.ne fail
    mrs x0,spsr_el1
    tst x0,#15
    b.ne fail
    mrs x0,elr_el2
    ldr x1,=0x3000+relay_after-service_vectors
    cmp x0,x1
    b.ne fail
    ldr x0,[sp,#64]
    cmp x0,#4093
    b.eq clone
    cmp x0,#4092
    b.eq child_done
    cmp x0,#4091
    b.eq parent_done
    b fail
clone:
    adr x0,phase
    ldr x1,[x0]
    cbnz x1,fail
    mov x1,#1
    str x1,[x0]
    mrs x0,elr_el1
    ldr x1,=0x1000+after_clone-user_code
    cmp x0,x1
    b.ne fail
    adr x1,snapshot
    mov x2,sp
    mov x3,#32
2:  ldr x4,[x2],#8
    str x4,[x1],#8
    subs x3,x3,#1
    b.ne 2b
    mrs x0,elr_el1
    mrs x1,spsr_el1
    mrs x2,sp_el0
    adr x3,saved_sys
    stp x0,x1,[x3]
    str x2,[x3,#16]
    adr x0,pages
    add x1,x0,#1,lsl #12
    mov x2,#512
3:  ldr x3,[x0],#8
    str x3,[x1],#8
    subs x2,x2,#1
    b.ne 3b
    dsb ish
    mov x0,#1
    bl switch_root
    str xzr,[sp] /* child resumes same PC, return zero */
    b restore
child_done:
    adr x0,phase
    ldr x1,[x0]
    cmp x1,#1
    b.ne fail
    mov x1,#2
    str x1,[x0]
    adr x0,pages
    ldr x1,[x0]
    cmp x1,#0x123
    b.ne fail
    add x0,x0,#1,lsl #12
    ldr x1,[x0]
    cmp x1,#0x456
    b.ne fail
    mov x0,#0
    bl switch_root
    adr x0,snapshot
    mov x1,sp
    mov x2,#32
4:  ldr x3,[x0],#8
    str x3,[x1],#8
    subs x2,x2,#1
    b.ne 4b
    adr x0,saved_sys
    ldp x1,x2,[x0]
    ldr x3,[x0,#16]
    msr elr_el1,x1
    msr spsr_el1,x2
    msr sp_el0,x3
    mov x0,#2001 /* synthetic parent return, not native PID */
    str x0,[sp]
    b restore
parent_done:
    adr x0,phase
    ldr x1,[x0]
    cmp x1,#2
    b.ne fail
    adr x0,pages
    ldr x1,[x0]
    cmp x1,#0x789
    b.ne fail
    add x0,x0,#1,lsl #12
    ldr x1,[x0]
    cmp x1,#0x456
    b.ne fail
    adr x1,passed
    b report
restore:
    ldp x0,x1,[sp,#0]
    ldp x2,x3,[sp,#16]
    ldp x4,x5,[sp,#32]
    ldp x6,x7,[sp,#48]
    ldp x8,x9,[sp,#64]
    ldp x10,x11,[sp,#80]
    ldp x12,x13,[sp,#96]
    ldp x14,x15,[sp,#112]
    ldp x16,x17,[sp,#128]
    ldp x18,x19,[sp,#144]
    ldp x20,x21,[sp,#160]
    ldp x22,x23,[sp,#176]
    ldp x24,x25,[sp,#192]
    ldp x26,x27,[sp,#208]
    ldp x28,x29,[sp,#224]
    ldr x30,[sp,#240]
    add sp,sp,#256
    eret
blocked:
    adr x1,blocked_msg
    b report
fail:
    adr x1,failed
report:
    mov x2,#0x09000000
5:  ldrb w3,[x1],#1
    cbz w3,7f
6:  ldr w4,[x2,#0x18]
    tbnz w4,#5,6b
    str w3,[x2]
    b 5b
7:  wfe
    b 7b
    .ltorg
.section .text.guest,"ax"
.balign 4096
user_code:
    mov x19,#0x123
    mov x20,#0xabc
    mov x30,#0xdef
    mov x2,#0x2000
    str x19,[x2]
    str x20,[sp,#-16]!
    mov x8,#4093
    svc #0
after_clone:
    cmp x19,#0x123
    b.ne bad
    cmp x20,#0xabc
    b.ne bad
    cmp x30,#0xdef
    b.ne bad
    ldr x3,[sp],#16
    cmp x3,x20
    b.ne bad
    mov x2,#0x2000
    ldr x3,[x2]
    cmp x3,#0x123
    b.ne bad
    cbnz x0,parent
    mov x3,#0x456
    str x3,[x2]
    mov x8,#4092
    svc #0
    b bad
parent:
    cmp x0,#2001
    b.ne bad
    mov x3,#0x789
    str x3,[x2]
    mov x8,#4091
    svc #0
bad:
    mov x8,#4090
    svc #0
    b bad
.balign 4096
service_vectors:
.rept 8
    b relay_bad
    .balign 128
.endr
    hvc #0x44
relay_after:
    eret
    .balign 128
.rept 7
    b relay_bad
    .balign 128
.endr
relay_bad:
    hvc #0x43
    b relay_bad
.balign 4096
.section .rodata,"a"
passed: .asciz "EXODUS_CLONE_REF_PASS\n"
failed: .asciz "EXODUS_CLONE_REF_FAIL\n"
blocked_msg: .asciz "EXODUS_CLONE_REF_EL1_BLOCKED\n"
.section .bss,"aw",%nobits
.balign 4096
zero_start:
roots: .skip 24576
s1roots: .skip 24576
pages: .skip 8192
phase: .skip 8
snapshot: .skip 256
saved_sys: .skip 24
zero_end:
.balign 16
stack: .skip 4096
stack_top:

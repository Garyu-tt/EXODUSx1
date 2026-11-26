/* Synthetic FreeBSD userspace PID 1, QEMU only. No production init. */
.section .text,"ax"
.global _start
_start:
    mov x8, #20
    svc #0
    cmp x0, #1
    b.ne halt
    adr x0, console
    mov x1, #1
    mov x2, #0
    mov x8, #5
    svc #0
    b.cs halt
    adr x1, marker
    mov x2, #marker_end-marker
    mov x8, #4
    svc #0
halt:
    adr x0, interval
    mov x1, #0
    mov x8, #240
    svc #0
    b halt
console: .asciz "/dev/console"
marker: .ascii "TUMOR_BAO_REF: FREEBSD_SYNTHETIC_PID1\n"
marker_end:
.balign 8
interval: .quad 1, 0
.section .note.ABI-tag,"a",%note
.balign 4
.long 8, 4, 1
.asciz "FreeBSD"
.balign 4
.long 1501000
.balign 4

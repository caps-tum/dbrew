//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    inc rax
    inc rbp
    inc r9
    inc qword ptr [r9]
    inc qword ptr [r9 + rax]
    inc qword ptr [r9 + 1 * rax]
    inc qword ptr [r9 + 2 * rdx]
    inc qword ptr [r9 + 4 * rdx]
    inc qword ptr [r9 + 8 * rdx]
    inc qword ptr [0x10 + r9]
    inc qword ptr [0x10 + r9 + r14]
    inc qword ptr [0x10 + r9 + 1 * r14]
    inc qword ptr [0x10 + r9 + 2 * r14]
    inc qword ptr [0x10 + r9 + 4 * r14]
    inc qword ptr [0x10 + r9 + 8 * r14]
    inc qword ptr [0x1000 + r9]
    inc qword ptr [0x1000 + r9 + rax]
    inc qword ptr [0x1000 + r9 + 1 * rax]
    inc qword ptr [0x1000 + r9 + 2 * rdx]
    inc qword ptr [0x1000 + r9 + 4 * rdx]
    inc qword ptr [0x1000 + r9 + 8 * rdx]
    inc qword ptr [0x1000 + 2 * rdx]
    inc qword ptr [0x1000 + 4 * rdx]
    inc qword ptr [0x1000 + 8 * rdx]
    inc qword ptr [f1 + rip]
    inc qword ptr [f1 + 0x100 + rip]

    add rbp, rax
    add rbp, rbp
    add rbp, r9
    add rbp, qword ptr [r9]
    add rbp, qword ptr [r9 + rax]
    add rbp, qword ptr [r9 + 1 * rax]
    add rbp, qword ptr [r9 + 2 * rdx]
    add rbp, qword ptr [r9 + 4 * rdx]
    add rbp, qword ptr [r9 + 8 * rdx]
    add rbp, qword ptr [0x10 + r9]
    add rbp, qword ptr [0x10 + r9 + r14]
    add rbp, qword ptr [0x10 + r9 + 1 * r14]
    add rbp, qword ptr [0x10 + r9 + 2 * r14]
    add rbp, qword ptr [0x10 + r9 + 4 * r14]
    add rbp, qword ptr [0x10 + r9 + 8 * r14]
    add rbp, qword ptr [0x1000 + r9]
    add rbp, qword ptr [0x1000 + r9 + rax]
    add rbp, qword ptr [0x1000 + r9 + 1 * rax]
    add rbp, qword ptr [0x1000 + r9 + 2 * rdx]
    add rbp, qword ptr [0x1000 + r9 + 4 * rdx]
    add rbp, qword ptr [0x1000 + r9 + 8 * rdx]
    add rbp, qword ptr [0x1000 + 2 * r15]
    add rbp, qword ptr [0x1000 + 4 * r15]
    add rbp, qword ptr [0x1000 + 8 * r15]
    add rbp, qword ptr [f1 + rip]
    add rbp, qword ptr [f1 + 0x100 + rip]

    add r9, rax
    add r9, rbp
    add r9, r9
    add r9, qword ptr [r9]
    add r9, qword ptr [r9 + rax]
    add r9, qword ptr [r9 + 1 * rax]
    add r9, qword ptr [r9 + 2 * rdx]
    add r9, qword ptr [r9 + 4 * rdx]
    add r9, qword ptr [r9 + 8 * rdx]
    add r9, qword ptr [0x10 + r9]
    add r9, qword ptr [0x10 + r9 + r14]
    add r9, qword ptr [0x10 + r9 + 1 * r14]
    add r9, qword ptr [0x10 + r9 + 2 * r14]
    add r9, qword ptr [0x10 + r9 + 4 * r14]
    add r9, qword ptr [0x10 + r9 + 8 * r14]
    add r9, qword ptr [0x1000 + r9]
    add r9, qword ptr [0x1000 + r9 + rax]
    add r9, qword ptr [0x1000 + r9 + 1 * rax]
    add r9, qword ptr [0x1000 + r9 + 2 * rdx]
    add r9, qword ptr [0x1000 + r9 + 4 * rdx]
    add r9, qword ptr [0x1000 + r9 + 8 * rdx]
    add r9, qword ptr [0x1000 + 2 * r15]
    add r9, qword ptr [0x1000 + 4 * r15]
    add r9, qword ptr [0x1000 + 8 * r15]
    add r9, qword ptr [f1 + rip]
    add r9, qword ptr [f1 + 0x100 + rip]

    mov r9, qword ptr [rsp + 8 * rax]
    mov r9, qword ptr [rbp + 8 * rax]
    mov r9, qword ptr [rsi + 8 * rax]
    mov r9, qword ptr [r12 + 8 * rax]
    mov r9, qword ptr [r13 + 8 * rax]
    mov r9, qword ptr [r14 + 8 * rax]
    mov r9, qword ptr [0x12345678 + 8 * rax]
    .byte 0x4d,0x8b,0x0c,0xc5,0x12,0x34,0x56,0x78

    add r9, [0x8 + rbx]
    add r9, [0x8 + rsp]
    add r9, [0x8 + rbp]
    add r9, [0x8 + r11]
    add r9, [0x8 + r12]
    add r9, [0x8 + r13]

    mov r9, qword ptr [rsp]
    mov r9, qword ptr [rbp]
    mov r9, qword ptr [rsi]
    mov r9, qword ptr [r12]
    mov r9, qword ptr [r13]
    mov r9, qword ptr [r14]

    lea r9, [0x1000 + r9 + 8 * r12]

    ret


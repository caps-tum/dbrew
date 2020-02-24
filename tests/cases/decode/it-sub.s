//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // SUB instructions, Intel Vol. 2B 4-407
    sub [rdi], rax
    sub [rdi], r9
    sub [rdi], eax
    sub [rdi], r9d
    sub [rdi], ax
    sub [rdi], r9w
    sub [rdi], al
    sub [rdi], r9b
    sub rax, [rdi]
    sub r9, [rdi]
    sub eax, [rdi]
    sub r9d, [rdi]
    sub ax, [rdi]
    sub r9w, [rdi]
    sub al, [rdi]
    sub r9b, [rdi]
    sub al, 0x10
    sub ax, 0x1000
    sub eax, 0xabcdef00
    sub rax, 0x0bcdef00
    sub byte ptr [rax], 0x10
    sub word ptr [rax], 0x310
    sub dword ptr [rax], 0x310
    sub qword ptr [rax], 0x310

    ret

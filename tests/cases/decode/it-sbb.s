//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // SBB instructions, Intel Vol. 2B 4-349
    sbb [rdi], rax
    sbb [rdi], r9
    sbb [rdi], eax
    sbb [rdi], r9d
    sbb [rdi], ax
    sbb [rdi], r9w
    sbb [rdi], al
    sbb [rdi], r9b
    sbb rax, [rdi]
    sbb r9, [rdi]
    sbb eax, [rdi]
    sbb r9d, [rdi]
    sbb ax, [rdi]
    sbb r9w, [rdi]
    sbb al, [rdi]
    sbb r9b, [rdi]
    sbb al, 0x10
    sbb ax, 0x1000
    sbb eax, 0xabcdef00
    sbb rax, 0x0bcdef00
    sbb byte ptr [rax], 0x10
    sbb word ptr [rax], 0x310
    sbb dword ptr [rax], 0x310
    sbb qword ptr [rax], 0x310

    ret

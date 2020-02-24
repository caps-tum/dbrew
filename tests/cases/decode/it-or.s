//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // OR instructions, Intel Vol. 2B 4-12
    or [rdi], rax
    or [rdi], r9
    or [rdi], eax
    or [rdi], r9d
    or [rdi], ax
    or [rdi], r9w
    or [rdi], al
    or [rdi], r9b
    or rax, [rdi]
    or r9, [rdi]
    or eax, [rdi]
    or r9d, [rdi]
    or ax, [rdi]
    or r9w, [rdi]
    or al, [rdi]
    or r9b, [rdi]
    or al, 0x10
    or ax, 0x1000
    or eax, 0xabcdef00
    or rax, 0x0bcdef00
    or byte ptr [rax], 0x10
    or word ptr [rax], 0x310
    or dword ptr [rax], 0x310
    or qword ptr [rax], 0x310

    ret


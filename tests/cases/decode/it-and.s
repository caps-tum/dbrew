//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // AND instructions, Intel Vol. 2A 3-55
    and [rdi], rax
    and [rdi], r9
    and [rdi], eax
    and [rdi], r9d
    and [rdi], ax
    and [rdi], r9w
    and [rdi], al
    and [rdi], r9b
    and rax, [rdi]
    and r9, [rdi]
    and eax, [rdi]
    and r9d, [rdi]
    and ax, [rdi]
    and r9w, [rdi]
    and al, [rdi]
    and r9b, [rdi]
    and al, 0x10
    and ax, 0x1000
    and eax, 0xabcdef00
    and rax, 0x0bcdef00
    and byte ptr [rax], 0x10
    and word ptr [rax], 0x310
    and dword ptr [rax], 0x310
    and qword ptr [rax], 0x310

    ret

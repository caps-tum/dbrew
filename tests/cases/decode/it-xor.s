//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // XOR instructions, Intel Vol. 2B 4-593
    xor [rdi], rax
    xor [rdi], r9
    xor [rdi], eax
    xor [rdi], r9d
    xor [rdi], ax
    xor [rdi], r9w
    xor [rdi], al
    xor [rdi], r9b
    xor rax, [rdi]
    xor r9, [rdi]
    xor eax, [rdi]
    xor r9d, [rdi]
    xor ax, [rdi]
    xor r9w, [rdi]
    xor al, [rdi]
    xor r9b, [rdi]
    xor al, 0x10
    xor ax, 0x1000
    xor eax, 0xabcdef00
    xor rax, 0x0bcdef00
    xor byte ptr [rax], 0x10
    xor word ptr [rax], 0x310
    xor dword ptr [rax], 0x310
    xor qword ptr [rax], 0x310

    ret

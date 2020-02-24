//!driver = test-driver-decode-buferr.c
//!cc = gcc
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // ADD instructions, Intel Vol. 2A 3-29
    add [rdi], rax
    add [rdi], r9
    add [rdi], eax
    add [rdi], r9d
    add [rdi], ax
    add [rdi], r9w
    add [rdi], al
    add [rdi], r9b
    add rax, [rdi]
    add r9, [rdi]
    add eax, [rdi]
    add r9d, [rdi]
    add ax, [rdi]
    add r9w, [rdi]
    add al, [rdi]
    add r9b, [rdi]
    add al, 0x10
    add ax, 0x1000
    add ax, 0x10
    add eax, 0xabcdef00
    add eax, 0x10
    add rax, 0x0bcdef00
    add rax, 0x10
    add byte ptr [rax], 0x10
    add word ptr [rax], 0x310
    add dword ptr [rax], 0x310
    add qword ptr [rax], 0x310

    ret

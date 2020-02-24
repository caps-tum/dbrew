//!driver = test-driver-decode.c
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // CMP instructions, Intel Vol. 2A 3-143
    cmp [rdi], rax
    cmp [rdi], r9
    cmp [rdi], eax
    cmp [rdi], r9d
    cmp [rdi], ax
    cmp [rdi], r9w
    cmp [rdi], al
    cmp [rdi], r9b
    cmp rax, [rdi]
    cmp r9, [rdi]
    cmp eax, [rdi]
    cmp r9d, [rdi]
    cmp ax, [rdi]
    cmp r9w, [rdi]
    cmp al, [rdi]
    cmp r9b, [rdi]
    cmp al, 0x10
    cmp ax, 0x1000
    cmp eax, 0xabcdef00
    cmp rax, 0x0bcdef00
    cmp byte ptr [rax], 0x10
    cmp word ptr [rax], 0x310
    cmp dword ptr [rax], 0x310
    cmp qword ptr [rax], 0x310

    ret

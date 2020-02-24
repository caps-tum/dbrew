//!driver = test-driver-decode.c
//!cc = gcc
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // MOVZX instructions, Intel Vol. 2A 3-604
    movzx bx, byte ptr [rdi]
    movzx ebx, byte ptr [rdi]
    movzx rcx, bl
    movzx ebx, word ptr [rdi]
    movzx rcx, bx

    ret

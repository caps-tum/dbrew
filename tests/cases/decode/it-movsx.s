//!driver = test-driver-decode.c
//!cc = gcc
.intel_syntax noprefix
    .text
    .globl  f1
    .type   f1, @function
f1:
    // MOVSX instructions, Intel Vol. 2A 3-598
    movsx bx, byte ptr [rdi]
    movsx ebx, byte ptr [rdi]
    movsx rcx, bl
    movsx ebx, word ptr [rdi]
    movsx rcx, bx
    movsx rcx, ebx

    ret
